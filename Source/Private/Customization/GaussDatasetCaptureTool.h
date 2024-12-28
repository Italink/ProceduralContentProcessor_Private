#pragma once

#include "ProceduralContentProcessor.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GaussDatasetCaptureTool.generated.h"

UCLASS(EditInlineNew, CollapseCategories, config = ProceduralContentProcessor, defaultconfig, Category = "Model", meta = (DisplayName = "Gauss Dataset Capture Tool"))
class PROCEDURALCONTENTPROCESSOR_API UGaussDatasetCaptureTool: public UProceduralWorldProcessor {
	GENERATED_BODY()
private:
	virtual void Activate() override;

	virtual void Deactivate() override;

	virtual void Tick(const float InDeltaTime) override;

	UFUNCTION(CallInEditor, Category = "Step0-GenerateDatabase")
	void Capture();

	UFUNCTION(CallInEditor, Category = "Step0-GenerateDatabase")
	void BrowseToDatabase();

	UFUNCTION(CallInEditor, Category = "Step1-ReconstructSparseModel")
	void ReconstructSparseModel();
	
	void OnActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh);

	void UpdateCameraMatrix();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	UPROPERTY(EditAnywhere, Category = "Step0-GenerateDatabase")
	TArray<TObjectPtr<AActor>> SourceActors;

	UPROPERTY(VisibleAnywhere, Category = "Step0-GenerateDatabase")
	TObjectPtr<ASceneCapture2D> CaptureActor;

	UPROPERTY(VisibleAnywhere, Category = "Step0-GenerateDatabase")
	TObjectPtr<UTextureRenderTarget2D> CaptureRT;

	UPROPERTY(EditAnywhere, Config, Category = "Step0-GenerateDatabase")
	int FrameXY = 16;

	UPROPERTY(EditAnywhere, Config, Category = "Step0-GenerateDatabase", meta = (UIMin = 0.01, ClampMin = 0.01, UIMax = 2, ClampMax = 2))
	float CaptureDistanceScale = 1.0f;

	FDelegateHandle OnActorSelectionChangedHandle;

	FBoxSphereBounds CurrentBounds;
};