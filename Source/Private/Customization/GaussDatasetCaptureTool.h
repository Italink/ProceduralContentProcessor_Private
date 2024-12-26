#pragma once

#include "ProceduralContentProcessor.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GaussDatasetCaptureTool.generated.h"

UCLASS(EditInlineNew, CollapseCategories, config = ProceduralContentProcessor, defaultconfig, Category = "Model", meta = (DisplayName = "Frame Animation Capture Tool"))
class PROCEDURALCONTENTPROCESSOR_API UGaussDatasetCaptureTool: public UProceduralWorldProcessor {
	GENERATED_BODY()
private:
	virtual void Activate() override;

	virtual void Deactivate() override;

	virtual void Tick(const float InDeltaTime) override;

	UFUNCTION(CallInEditor)
	void Capture();

	UFUNCTION(CallInEditor)
	void CreateAsset();

	void OnActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh);
private:
	UPROPERTY(EditAnywhere)
	TArray<TObjectPtr<AActor>> SourceActors;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<ASceneCapture2D> CaptureActor;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextureRenderTarget2D> CaptureRT;

	UPROPERTY(EditAnywhere, meta = (UIMin = 0.01, ClampMin = 0.01, UIMax = 2, ClampMax = 2))
	float BoundsScaleFactor = 1.0f;

	UPROPERTY(EditAnywhere, Config)
	int FrameCount = 32;

	UPROPERTY(EditAnywhere, Config)
	int FrameStep = 1;

	UPROPERTY(EditAnywhere, Config)
	bool bPaddingToPowerOfTwo = true;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTexture2D> FrameTexture;

	UPROPERTY(Config)
	FString LastSavePath;

	FDelegateHandle OnActorSelectionChangedHandle;
};