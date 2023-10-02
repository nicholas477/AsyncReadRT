// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "SceneViewExtension.h"

class FAsyncReadRTViewExtension final : public FSceneViewExtensionBase
{
public:
	FAsyncReadRTViewExtension(const FAutoRegister& AutoRegister);

	//~ ISceneViewExtension interface
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) {};
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) {};
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) {};
	virtual void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) {};
	virtual void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) {};
	void OnBackBufferReady_RenderThread(SWindow& SlateWindow, const FTexture2DRHIRef& BackBuffer);

	/**
	 * Allows to render content after the 3D content scene, useful for debugging
	 */
	virtual void PostRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) override;

	/**
	 * Allows to render content after the 3D content scene, useful for debugging
	 */
	virtual void PostRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) override;

	void AddRenderViewCallback(TFunction<void()> Callback)
	{
		PostRenderViewCallbacks.Enqueue(Callback);
	}

protected:
	TQueue<TFunction<void()>, EQueueMode::Mpsc> PostRenderViewCallbacks;
	void CallCallbacks();
};

class FAsyncReadRTModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static FAsyncReadRTModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FAsyncReadRTModule>("AsyncReadRT");
	}

	TSharedPtr<FAsyncReadRTViewExtension, ESPMode::ThreadSafe> GetViewExtension() const
	{
		return ViewExtension;
	}

protected:
	TSharedPtr<FAsyncReadRTViewExtension, ESPMode::ThreadSafe> ViewExtension;
};
