// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "AsyncReadRTAction.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAsyncReadRTOutputPin, FLinearColor, Color);

class UTextureRenderTarget2D;

struct FAsyncReadRTData
{
	FGPUFenceRHIRef TextureFence;
	FTexture2DRHIRef Texture;
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
	bool bFinished = false;

	UPROPERTY(BlueprintAssignable)
		FAsyncReadRTOutputPin OnReadRenderTarget;

	TSharedPtr<FAsyncReadRTData> ReadRTData;

protected:
	UFUNCTION()
		void OnNextFrame();

	uint32 StartFrame;
};
