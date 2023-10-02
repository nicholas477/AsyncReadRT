// Copyright Epic Games, Inc. All Rights Reserved.

#include "AsyncReadRT.h"

#define LOCTEXT_NAMESPACE "FAsyncReadRTModule"

void FAsyncReadRTModule::StartupModule()
{
	FCoreDelegates::OnPostEngineInit.AddLambda([this]()
	{
		ViewExtension = FSceneViewExtensions::NewExtension<FAsyncReadRTViewExtension>();
	});
}

void FAsyncReadRTModule::ShutdownModule()
{
	ViewExtension.Reset();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FAsyncReadRTModule, AsyncReadRT)

FAsyncReadRTViewExtension::FAsyncReadRTViewExtension(const FAutoRegister& AutoRegister)
	: FSceneViewExtensionBase(AutoRegister)
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->OnBackBufferReadyToPresent().AddRaw(this, &FAsyncReadRTViewExtension::OnBackBufferReady_RenderThread);
	}
}

void FAsyncReadRTViewExtension::OnBackBufferReady_RenderThread(SWindow& SlateWindow, const FTexture2DRHIRef& BackBuffer)
{
	CallCallbacks();
}

void FAsyncReadRTViewExtension::PostRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily)
{
	//TFunction<void()> RenderCallback;
	//while (PostRenderViewCallbacks.Dequeue(RenderCallback))
	//{
	//	if (RenderCallback)
	//	{
	//		RenderCallback();
	//	}
	//}
}

void FAsyncReadRTViewExtension::PostRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView)
{
	
}

void FAsyncReadRTViewExtension::CallCallbacks()
{
	TFunction<void()> RenderCallback;
	while (PostRenderViewCallbacks.Dequeue(RenderCallback))
	{
		if (RenderCallback)
		{
			RenderCallback();
		}
	}
}
