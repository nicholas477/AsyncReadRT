// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "RHIResources.h"
#include "AsyncReadRTAreaAction.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAsyncReadRTAreaOutputPin, const TArray<FLinearColor>&, Color);

class UTextureRenderTarget2D;

struct FAsyncReadRTAreaData
{
	FGPUFenceRHIRef TextureFence;
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
	FTextureRHIRef Texture;
#else
	FTexture2DRHIRef Texture;
#endif
	TAtomic<bool> FinishedRead;
	TArray<FLinearColor> PixelColors;
};

/**
 * 
 */
UCLASS()
class ASYNCREADRT_API UAsyncReadRTAreaAction : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"), Category = "Rendering")
	static UAsyncReadRTAreaAction* AsyncReadRenderTargetArea(UObject* WorldContextObject, UTextureRenderTarget2D* TextureRenderTarget, int32 StartX, int32 StartY, int32 EndX, int32 EndY, bool bFlushRHI);

	// UBlueprintAsyncActionBase interface
	virtual void Activate() override;
	//~UBlueprintAsyncActionBase interface

	UPROPERTY()
	UObject* WorldContextObject;

	UPROPERTY()
	UTextureRenderTarget2D* RT;

	int32 StartX;
	int32 StartY;
	int32 EndX;
	int32 EndY;
	bool bFlushRHI;

	UPROPERTY(BlueprintAssignable)
	FAsyncReadRTAreaOutputPin OnReadRenderTargetArea;

	TSharedPtr<FAsyncReadRTAreaData, ESPMode::ThreadSafe> ReadRTData;

protected:
	UFUNCTION()
	void OnNextFrame();

	uint64 StartFrame;
};
