// Fill out your copyright notice in the Description page of Project Settings.


#include "AsyncReadRTAction.h"

#include "AsyncReadRT.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RenderGraphBuilder.h"
#include "Async/Async.h"

UAsyncReadRTAction* UAsyncReadRTAction::AsyncReadRenderTarget(UObject* WorldContextObject, UTextureRenderTarget2D* TextureRenderTarget, int32 X, int32 Y, bool bNormalize)
{
	UAsyncReadRTAction* BlueprintNode = NewObject<UAsyncReadRTAction>();
	BlueprintNode->WorldContextObject = WorldContextObject;
	BlueprintNode->RT = TextureRenderTarget;
	BlueprintNode->X = X;
	BlueprintNode->Y = Y;
	BlueprintNode->bNormalize = bNormalize;
	return BlueprintNode;
}

void UAsyncReadRTAction::Activate()
{
	FTextureRenderTarget2DResource* TextureResource = (FTextureRenderTarget2DResource*)RT->GetResource();
	check(TextureResource);
	check(TextureResource->GetRenderTargetTexture());

	X = FMath::Clamp(X, 0, RT->SizeX - 1);
	Y = FMath::Clamp(Y, 0, RT->SizeY - 1);

	FReadSurfaceDataFlags ReadSurfaceDataFlags = bNormalize ? FReadSurfaceDataFlags() : FReadSurfaceDataFlags(RCM_MinMax);


	//FAsyncReadRTModule::Get().GetViewExtension()->AddRenderViewCallback([=, AsyncReadPtr = TWeakObjectPtr<ThisClass>(this), TextureRHI = TextureResource->GetRenderTargetTexture()]()
	ENQUEUE_RENDER_COMMAND(FCopyRTAsync)([=, AsyncReadPtr = TWeakObjectPtr<ThisClass>(this), TextureRHI = TextureResource->GetRenderTargetTexture()](FRHICommandListImmediate& RHICmdList)
	{
		check(IsInRenderingThread());
		check(TextureRHI.IsValid());

		FGPUFenceRHIRef Fence = RHICreateGPUFence(TEXT("AsyncRTReadback"));

		SCOPED_NAMED_EVENT_TEXT("UAsyncReadRTAction::AsyncReadRT", FColor::Magenta);

		FRHIResourceCreateInfo CreateInfo(TEXT("AsyncRTReadback"));
		FTexture2DRHIRef IORHITextureCPU = RHICreateTexture2D(1, 1, TextureRHI->GetFormat(), 1, 1, TexCreate_CPUReadback, ERHIAccess::CopyDest, CreateInfo);

		{
			SCOPED_NAMED_EVENT_TEXT("UAsyncReadRTAction::AsyncReadRT::CopyTexture", FColor::Magenta);
			FRHICopyTextureInfo CopyTextureInfo;
			CopyTextureInfo.Size = FIntVector(1,1,1);
			CopyTextureInfo.SourceMipIndex = 0;
			CopyTextureInfo.DestMipIndex = 0;
			CopyTextureInfo.SourcePosition = FIntVector(X, Y, 0);
			CopyTextureInfo.DestPosition = FIntVector(0, 0, 0);

			RHICmdList.Transition(FRHITransitionInfo(IORHITextureCPU, ERHIAccess::Unknown, ERHIAccess::CopyDest));
			RHICmdList.Transition(FRHITransitionInfo(TextureRHI, ERHIAccess::Unknown, ERHIAccess::CopySrc));
			RHICmdList.CopyTexture(TextureRHI, IORHITextureCPU, CopyTextureInfo);

			RHICmdList.Transition(FRHITransitionInfo(IORHITextureCPU, ERHIAccess::CopyDest, ERHIAccess::CopySrc));
		}
		RHICmdList.WriteGPUFence(Fence);

		FLinearColor PixelColor;

		{
			SCOPED_NAMED_EVENT_TEXT("UAsyncReadRTAction::AsyncReadRT::MapTexture", FColor::Magenta);
			void* OutputBuffer = NULL;
			int32 Width; int32 Height;
			GDynamicRHI->RHIMapStagingSurface(IORHITextureCPU, Fence, OutputBuffer, Width, Height, RHICmdList.GetGPUMask().ToIndex());
			{
				switch (TextureRHI->GetFormat())
				{
				case EPixelFormat::PF_FloatRGBA:
				{
					FFloat16Color OutputColor;
					FMemory::Memcpy(&OutputColor, OutputBuffer, sizeof(OutputColor));
					PixelColor.R = OutputColor.R;
					PixelColor.G = OutputColor.G;
					PixelColor.B = OutputColor.B;
					PixelColor.A = OutputColor.A;
					break;
				}
				case EPixelFormat::PF_B8G8R8A8:
				{
					FColor OutputColor;
					FMemory::Memcpy(&OutputColor, OutputBuffer, sizeof(OutputColor));
					PixelColor.R = OutputColor.R;
					PixelColor.G = OutputColor.G;
					PixelColor.B = OutputColor.B;
					PixelColor.A = OutputColor.A;
					PixelColor /= 255.f;
					break;
				}
				default:
					UE_LOG(LogTemp, Warning, TEXT("UAsyncReadRTAction: Unsupported RT format! Format: %d"), static_cast<int32>(TextureRHI->GetFormat())); // Unsupported, add a new switch statement.
				}
			}
			RHICmdList.UnmapStagingSurface(IORHITextureCPU);
		}

		AsyncTask(ENamedThreads::GameThread, [PixelColor, AsyncReadPtr]()
			{
				if (AsyncReadPtr.IsValid())
				{
					AsyncReadPtr->OnReadRenderTarget.Broadcast(PixelColor);
				}
			});
	});
}