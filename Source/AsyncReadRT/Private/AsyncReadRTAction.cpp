// Fill out your copyright notice in the Description page of Project Settings.


#include "AsyncReadRTAction.h"

#include "AsyncReadRT.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RenderGraphBuilder.h"
#include "Async/Async.h"
#include "CoreGlobals.h"

UAsyncReadRTAction* UAsyncReadRTAction::AsyncReadRenderTarget(UObject* WorldContextObject, UTextureRenderTarget2D* TextureRenderTarget, int32 X, int32 Y, bool bFlushRHI)
{
	UAsyncReadRTAction* BlueprintNode = NewObject<UAsyncReadRTAction>();
	BlueprintNode->WorldContextObject = WorldContextObject;
	BlueprintNode->RT = TextureRenderTarget;
	BlueprintNode->X = X;
	BlueprintNode->Y = Y;
	BlueprintNode->bFlushRHI = bFlushRHI;
	BlueprintNode->ReadRTData = MakeShared<FAsyncReadRTData, ESPMode::ThreadSafe>();
	BlueprintNode->ReadRTData->FinishedRead = false;
	return BlueprintNode;
}

static void ReadPixel(int32 Width, int32 Height, void* Data, EPixelFormat Format, FLinearColor& OutColor)
{
	switch (Format)
	{
	case EPixelFormat::PF_FloatRGBA:
	{
		FFloat16Color* OutputColor = reinterpret_cast<FFloat16Color*>(Data);
		OutColor.R = OutputColor->R;
		OutColor.G = OutputColor->G;
		OutColor.B = OutputColor->B;
		OutColor.A = OutputColor->A;
		break;
	}
	case EPixelFormat::PF_B8G8R8A8:
	{
		FColor* OutputColor = reinterpret_cast<FColor*>(Data);
		OutColor.R = OutputColor->R;
		OutColor.G = OutputColor->G;
		OutColor.B = OutputColor->B;
		OutColor.A = OutputColor->A;
		OutColor /= 255.f;
		break;
	}
	default:
		UE_LOG(LogTemp, Warning, TEXT("UAsyncReadRTAction: Unsupported RT format! Format: %d"), static_cast<int32>(Format)); // Unsupported, add a new switch statement.
	}
}

// Maps the texture and reads a single pixel.
// If bFlushRHI is false, then it checks the render fence. If the render fence hasn't completed, then the function early exits
static void PollRTRead(FRHICommandListImmediate& RHICmdList, 
	TSharedPtr<FAsyncReadRTData, ESPMode::ThreadSafe> ReadData,
	TWeakObjectPtr<UAsyncReadRTAction> ReadAction, bool bFlushRHI)
{
	SCOPED_NAMED_EVENT_TEXT("AsyncReadRTAction::AsyncReadRT::PollRTRead", FColor::Magenta);

	check(IsInRenderingThread());

	// If we didn't flush the RHI then make sure the previous rendering commands got done
	if (!bFlushRHI)
	{
		// Return if we haven't finished the texture commands
		if (!ReadData->TextureFence.IsValid() || !ReadData->TextureFence->Poll())
		{
			return;
		}
	}

	SCOPED_NAMED_EVENT_TEXT("AsyncReadRTAction::AsyncReadRT::MapTexture", FColor::Magenta);
	void* OutputBuffer = NULL;
	int32 Width; int32 Height;

	if (bFlushRHI)
	{
		// This flushes the command list
		RHICmdList.MapStagingSurface(ReadData->Texture, ReadData->TextureFence, OutputBuffer, Width, Height);
	}
	else
	{
		GDynamicRHI->RHIMapStagingSurface(ReadData->Texture, ReadData->TextureFence, OutputBuffer, Width, Height, RHICmdList.GetGPUMask().ToIndex());
	}
	{
		ReadPixel(Width, Height, OutputBuffer, ReadData->Texture->GetFormat(), ReadData->PixelColor);
	}
	RHICmdList.UnmapStagingSurface(ReadData->Texture);
	ReadData->FinishedRead = true;
}

void UAsyncReadRTAction::Activate()
{
	FTextureRenderTarget2DResource* TextureResource = (FTextureRenderTarget2DResource*)RT->GetResource();
	check(TextureResource);
	check(TextureResource->GetRenderTargetTexture());

	X = FMath::Clamp(X, 0, RT->SizeX - 1);
	Y = FMath::Clamp(Y, 0, RT->SizeY - 1);

	StartFrame = GFrameCounter;

	ENQUEUE_RENDER_COMMAND(FCopyRTAsync)([=, AsyncReadPtr = TWeakObjectPtr<UAsyncReadRTAction>(this), TextureRHI = TextureResource->GetRenderTargetTexture(), ReadData = ReadRTData](FRHICommandListImmediate& RHICmdList)
	{
		check(IsInRenderingThread());
		check(TextureRHI.IsValid());

		FGPUFenceRHIRef Fence = RHICreateGPUFence(TEXT("AsyncRTReadback"));

		SCOPED_NAMED_EVENT_TEXT("AsyncReadRTAction::AsyncReadRT", FColor::Magenta);

		FTexture2DRHIRef IORHITextureCPU;
		{
			SCOPED_NAMED_EVENT_TEXT("AsyncReadRTAction::AsyncReadRT::CreateCopyTexture", FColor::Magenta);

			FRHIResourceCreateInfo CreateInfo(TEXT("AsyncRTReadback"));
			IORHITextureCPU = RHICreateTexture2D(1, 1, TextureRHI->GetFormat(), 1, 1, TexCreate_CPUReadback, ERHIAccess::CopyDest, CreateInfo);

			FRHICopyTextureInfo CopyTextureInfo;
			CopyTextureInfo.Size = FIntVector(1, 1, 1);
			CopyTextureInfo.SourceMipIndex = 0;
			CopyTextureInfo.DestMipIndex = 0;
			CopyTextureInfo.SourcePosition = FIntVector(X, Y, 0);
			CopyTextureInfo.DestPosition = FIntVector(0, 0, 0);

			RHICmdList.Transition(FRHITransitionInfo(TextureRHI, ERHIAccess::Unknown, ERHIAccess::CopySrc));
			RHICmdList.CopyTexture(TextureRHI, IORHITextureCPU, CopyTextureInfo);

			RHICmdList.Transition(FRHITransitionInfo(IORHITextureCPU, ERHIAccess::CopyDest, ERHIAccess::CopySrc));
			RHICmdList.WriteGPUFence(Fence);
		}
		check(Fence.IsValid());

		ReadData->Texture = IORHITextureCPU;
		ReadData->TextureFence = Fence;

		// If we flush the RHI then we can just go ahead and read the mapped texture asap
		if (bFlushRHI)
		{
			PollRTRead(RHICmdList, ReadData, AsyncReadPtr, bFlushRHI);
		}
	});

	WorldContextObject->GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UAsyncReadRTAction::OnNextFrame);
}

void UAsyncReadRTAction::OnNextFrame()
{
	//const int32 FramesWaited = GFrameCounter - StartFrame;
	//UE_LOG(LogTemp, Warning, TEXT("Frames waited: %d"), FramesWaited);

	check(ReadRTData.IsValid());

	if (ReadRTData->FinishedRead)
	{
		OnReadRenderTarget.Broadcast(ReadRTData->PixelColor);
		SetReadyToDestroy();
	}
	else
	{
		ENQUEUE_RENDER_COMMAND(FReadRTAsync)([WeakThis = TWeakObjectPtr<UAsyncReadRTAction>(this), ReadRTData = ReadRTData](FRHICommandListImmediate& RHICmdList)
		{
			PollRTRead(RHICmdList, ReadRTData, WeakThis, false);
		});

		WorldContextObject->GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UAsyncReadRTAction::OnNextFrame);
	}
}
