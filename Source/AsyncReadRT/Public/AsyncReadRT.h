// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "PixelFormat.h"

class FAsyncReadRTModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

static void ReadPixel(int32 Width, int32 Height, void* Data,
                      EPixelFormat Format, FLinearColor& OutColor)
{
	switch (Format)
	{
	case EPixelFormat::PF_R8:
	// unsure why R8 is showing up as G8 but done both here just in case it
	// changes in the future.
	case EPixelFormat::PF_G8:
		{
			uint8* OutputColor = reinterpret_cast<uint8*>(Data);
			float Gray = static_cast<float>(*OutputColor) / 255.f;
			OutColor.R = Gray;
			OutColor.G = Gray;
			OutColor.B = Gray;
			OutColor.A = 1.f;
			break;
		}

	case EPixelFormat::PF_R16F:
		{
			FFloat16* OutputColor = reinterpret_cast<FFloat16*>(Data);
			OutColor.R = FMath::Clamp(OutputColor->GetFloat(), 0.f, 1.f);
			OutColor.G = FMath::Clamp(OutputColor->GetFloat(), 0.f, 1.f);

			OutColor.B = FMath::Clamp(OutputColor->GetFloat(), 0.f, 1.f);
			OutColor.A = FMath::Clamp(OutputColor->GetFloat(), 0.f, 1.f);
			break;
		}

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
		UE_LOG(LogTemp, Warning,
		       TEXT("UAsyncReadRTAction: Unsupported RT format! Format: %d"),
		       static_cast<int32>(
			       Format)); // Unsupported, add a new switch statement.
	}
}
