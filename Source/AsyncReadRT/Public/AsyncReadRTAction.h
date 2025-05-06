// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "RHIResources.h"
#include "AsyncReadRTAction.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAsyncReadRTOutputPin, FLinearColor, Color);

class UTextureRenderTarget2D;

struct FAsyncReadRTData
{
	FGPUFenceRHIRef TextureFence;
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
	FTextureRHIRef Texture;
#else
	FTexture2DRHIRef Texture;
#endif
	TAtomic<bool> FinishedRead;
	FLinearColor PixelColor;
};

/**
 * 
 */
UCLASS()
class ASYNCREADRT_API UAsyncReadRTAction : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"), Category = "Rendering")
		static UAsyncReadRTAction* AsyncReadRenderTarget(UObject* WorldContextObject, UTextureRenderTarget2D* TextureRenderTarget, int32 X, int32 Y, bool bFlushRHI);

	// UBlueprintAsyncActionBase interface
	virtual void Activate() override;
	//~UBlueprintAsyncActionBase interface

	UPROPERTY()
		UObject* WorldContextObject;

	UPROPERTY()
		UTextureRenderTarget2D* RT;

	int32 X;
	int32 Y;
	bool bFlushRHI;

	UPROPERTY(BlueprintAssignable)
		FAsyncReadRTOutputPin OnReadRenderTarget;

	TSharedPtr<FAsyncReadRTData, ESPMode::ThreadSafe> ReadRTData;

protected:
	UFUNCTION()
		void OnNextFrame();

	uint64 StartFrame;
};
