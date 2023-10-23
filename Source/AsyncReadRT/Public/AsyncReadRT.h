// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FAsyncReadRTModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

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