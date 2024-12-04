#pragma once

#include "ProceduralContentProcessor.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ShadowProxyEditor.generated.h"

UCLASS(EditInlineNew, CollapseCategories, config = ProceduralContentProcessor, defaultconfig, Category = "WorldPartition", meta = (DisplayName = "Shadow Proxy Editor"))
class PROCEDURALCONTENTPROCESSOR_API UShadowProxyEditor: public UProceduralWorldProcessor {
	GENERATED_BODY()
private:
	virtual void Activate() override;

	virtual void Deactivate() override;

	virtual void Tick(const float InDeltaTime) override;

	UFUNCTION(CallInEditor)
	void Capture();

	void OnActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh);

	void OnRefreshLightActors();

private:
	UPROPERTY(EditAnywhere)
	TArray<TObjectPtr<AActor>> SourceActors;

	UPROPERTY(EditAnywhere)
	TArray<TObjectPtr<AActor>> LightActors;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<ASceneCapture2D> CaptureActor;

	UPROPERTY(EditAnywhere)
	int32 CaptureResolution = 1024;

	UPROPERTY(EditAnywhere, meta = (UIMin = 0.01, ClampMin = 0.01, UIMax = 1, ClampMax = 1))
	float CaptureScale = 1.0f;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextureRenderTarget2D> CaptureRT;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMesh> CaptureResult;

	UPROPERTY(EditAnywhere, Config)
	FSoftObjectPath BillboardMaterial;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UMaterialInstanceDynamic> BillboardMaterialMID;

	FDelegateHandle OnActorSelectionChangedHandle;

	FBoxSphereBounds CurrentBounds;
};