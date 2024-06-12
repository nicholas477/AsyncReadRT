// Fill out your copyright notice in the Description page of Project Settings.


#include "AsyncReadEntireRTAction.h"

#include "AsyncReadRT.h"
#include "RHICommandList.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RenderGraphBuilder.h"
#include "Async/Async.h"
#include "CoreGlobals.h"
#include "TextureResource.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Engine/World.h"
#include "TimerManager.h"

UAsyncReadEntireRTAction* UAsyncReadEntireRTAction::AsyncReadEntireRenderTarget(UObject* WorldContextObject, UTextureRenderTarget2D* TextureRenderTarget, bool bFlushRHI)
{
	UAsyncReadEntireRTAction* BlueprintNode = NewObject<UAsyncReadEntireRTAction>();
	BlueprintNode->WorldContextObject = WorldContextObject;
	BlueprintNode->RT = TextureRenderTarget;
	BlueprintNode->bFlushRHI = bFlushRHI;
	BlueprintNode->ReadRTData = MakeShared<FAsyncReadEntireRTData, ESPMode::ThreadSafe>();
	BlueprintNode->ReadRTData->FinishedRead = false;
	return BlueprintNode;
}

// Maps the texture and reads a single pixel.
// If bFlushRHI is false, then it checks the render fence. If the render fence hasn't completed, then the function early exits
static void PollRTRead(FRHICommandListImmediate& RHICmdList,
	TSharedPtr<FAsyncReadEntireRTData, ESPMode::ThreadSafe> ReadData,
	TWeakObjectPtr<UAsyncReadEntireRTAction> ReadAction, bool bFlushRHI)
{
	SCOPED_NAMED_EVENT_TEXT("AsyncReadEntireRTAction::AsyncReadRT::PollRTRead", FColor::Magenta);

	check(IsInRenderingThread());
	ReadData->FinishedRead = false;

	// If we didn't flush the RHI then make sure the previous rendering commands got done
	if (!bFlushRHI)
	{
		// Return if we haven't finished the texture commands
		if (!ReadData->TextureFence.IsValid() || !ReadData->TextureFence->Poll())
		{
			return;
		}
	}

	SCOPED_NAMED_EVENT_TEXT("AsyncReadEntireRTAction::AsyncReadRT::MapTexture", FColor::Magenta);
	void* OutputBuffer = NULL;
	int32 RowPitchInPixels, Height;

	if (bFlushRHI)
	{
		// This flushes the command list
		RHICmdList.MapStagingSurface(ReadData->Texture, ReadData->TextureFence, OutputBuffer, RowPitchInPixels, Height);
	}
	else
	{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 2
		GDynamicRHI->RHIMapStagingSurface_RenderThread(RHICmdList, ReadData->Texture, INDEX_NONE, ReadData->TextureFence, OutputBuffer, RowPitchInPixels, Height);
#else
		GDynamicRHI->RHIMapStagingSurface_RenderThread(RHICmdList, ReadData->Texture, ReadData->TextureFence, OutputBuffer, RowPitchInPixels, Height);
#endif
	}

	const int32 Width = ReadData->Texture->GetSizeX();
	check(RowPitchInPixels >= Width);
	check(Height == ReadData->Texture->GetSizeY());
	const int32 SrcPitch = RowPitchInPixels * GPixelFormats[ReadData->Texture->GetFormat()].BlockBytes;

	ReadData->PixelColors.Empty(Width * Height);

	const EPixelFormat Format = ReadData->Texture->GetFormat();
	for (int32 YIndex = 0; YIndex < Height; YIndex++)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			const int32 PixelOffset = X + (YIndex * RowPitchInPixels);

			FLinearColor& OutColor = ReadData->PixelColors.AddDefaulted_GetRef();
			switch (Format)
			{
			case EPixelFormat::PF_FloatRGBA:
			{
				FFloat16Color* OutputColor = reinterpret_cast<FFloat16Color*>(OutputBuffer) + PixelOffset;
				OutColor.R = OutputColor->R;
				OutColor.G = OutputColor->G;
				OutColor.B = OutputColor->B;
				OutColor.A = OutputColor->A;
				break;
			}
			case EPixelFormat::PF_B8G8R8A8:
			{
				FColor* OutputColor = reinterpret_cast<FColor*>(OutputBuffer) + PixelOffset;
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
	}
	RHICmdList.UnmapStagingSurface(ReadData->Texture);
	ReadData->FinishedRead = true;
}

void UAsyncReadEntireRTAction::Activate()
{
	FTextureRenderTarget2DResource* TextureResource = (FTextureRenderTarget2DResource*)RT->GetResource();
	check(TextureResource);
	check(TextureResource->GetRenderTargetTexture());

	StartFrame = GFrameCounter;

	ENQUEUE_RENDER_COMMAND(FCopyRTAsync)([bFlushRHI = bFlushRHI, AsyncReadPtr = TWeakObjectPtr<UAsyncReadEntireRTAction>(this), TextureRHI = TextureResource->GetRenderTargetTexture(), ReadData = ReadRTData](FRHICommandListImmediate& RHICmdList)
		{
			check(IsInRenderingThread());
			check(TextureRHI.IsValid());

			FGPUFenceRHIRef Fence = RHICreateGPUFence(TEXT("AsyncEntireRTReadback"));

			SCOPED_NAMED_EVENT_TEXT("AsyncReadEntireRTAction::AsyncReadRT", FColor::Magenta);

			FTexture2DRHIRef IORHITextureCPU;
			{
				SCOPED_NAMED_EVENT_TEXT("AsyncReadEntireRTAction::AsyncReadRT::CreateCopyTexture", FColor::Magenta);

				int32 Width, Height;
				Width = TextureRHI->GetSizeX();
				Height = TextureRHI->GetSizeY();
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 2
				FRHITextureCreateDesc TextureDesc = FRHITextureCreateDesc::Create2D(TEXT("AsyncEntireRTReadback"), Width, Height, TextureRHI->GetFormat());
				TextureDesc.AddFlags(ETextureCreateFlags::CPUReadback);
				TextureDesc.InitialState = ERHIAccess::CopyDest;
#if ENGINE_MINOR_VERSION > 3
				IORHITextureCPU = GDynamicRHI->RHICreateTexture(FRHICommandListExecutor::GetImmediateCommandList(), TextureDesc);
#else // ENGINE_MINOR_VERSION
				IORHITextureCPU = GDynamicRHI->RHICreateTexture(TextureDesc);
#endif // ENGINE_MINOR_VERSION
#else
				FRHIResourceCreateInfo CreateInfo(TEXT("AsyncRTReadback"));
				IORHITextureCPU = RHICreateTexture2D(Width, Height, TextureRHI->GetFormat(), 1, 1, TexCreate_CPUReadback, ERHIAccess::CopyDest, CreateInfo);
#endif

				FRHICopyTextureInfo CopyTextureInfo;
				CopyTextureInfo.Size = FIntVector(Width, Height, 1);
				CopyTextureInfo.SourceMipIndex = 0;
				CopyTextureInfo.DestMipIndex = 0;
				CopyTextureInfo.SourcePosition = FIntVector(0, 0, 0);
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

	WorldContextObject->GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UAsyncReadEntireRTAction::OnNextFrame);
}

void UAsyncReadEntireRTAction::OnNextFrame()
{
	//const int32 FramesWaited = GFrameCounter - StartFrame;
	//UE_LOG(LogTemp, Warning, TEXT("Frames waited: %d"), FramesWaited);

	check(IsInGameThread());
	check(ReadRTData.IsValid());

	if (ReadRTData->FinishedRead)
	{
		OnReadEntireRenderTarget.Broadcast(ReadRTData->PixelColors);
		SetReadyToDestroy();
	}
	else
	{
		ENQUEUE_RENDER_COMMAND(FReadRTAsync)([WeakThis = TWeakObjectPtr<UAsyncReadEntireRTAction>(this), ReadRTData = ReadRTData](FRHICommandListImmediate& RHICmdList)
		{
			PollRTRead(RHICmdList, ReadRTData, WeakThis, false);
		});

		WorldContextObject->GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UAsyncReadEntireRTAction::OnNextFrame);
	}
}
