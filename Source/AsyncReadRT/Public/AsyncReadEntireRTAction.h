// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "RHIResources.h"
#include "AsyncReadEntireRTAction.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAsyncReadEntireRTOutputPin, const TArray<FLinearColor>&, Colors);

class UTextureRenderTarget2D;

struct FAsyncReadEntireRTData
{
	FGPUFenceRHIRef TextureFence;
	FTexture2DRHIRef Texture;
	TAtomic<bool> FinishedRead;
	TArray<FLinearColor> PixelColors;
};

/**
 * 
 */
UCLASS()
class ASYNCREADRT_API UAsyncReadEntireRTAction : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"), Category = "Rendering")
		static UAsyncReadEntireRTAction* AsyncReadEntireRenderTarget(UObject* WorldContextObject, UTextureRenderTarget2D* TextureRenderTarget, bool bFlushRHI);

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
		FAsyncReadEntireRTOutputPin OnReadEntireRenderTarget;

	TSharedPtr<FAsyncReadEntireRTData, ESPMode::ThreadSafe> ReadRTData;

protected:
	UFUNCTION()
		void OnNextFrame();

	uint64 StartFrame;
};
