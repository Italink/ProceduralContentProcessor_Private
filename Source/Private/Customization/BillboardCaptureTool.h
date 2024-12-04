#pragma once

#include "ProceduralContentProcessor.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "BillboardCaptureTool.generated.h"


struct FQuadrangle {
public:
	FVector2D Vertices[4];
	FVector2D& BottomLeft() { return Vertices[0]; }
	FVector2D& BottomRight() { return Vertices[1]; }
	FVector2D& TopRight() { return Vertices[2]; }
	FVector2D& TopLeft() { return Vertices[3]; }

	static FMatrix CalcTransformMaterix(FQuadrangle InSrc, FQuadrangle InDst);
	static FVector2D TransformPoint(const FMatrix& InMat, const FVector2D& InPoint);
};


UCLASS(EditInlineNew, CollapseCategories, config = ProceduralContentProcessor, defaultconfig, Category = "Model", meta = (DisplayName = "Billboard Capture Tool"))
class PROCEDURALCONTENTPROCESSOR_API UBillboardCaptureTool : public UProceduralWorldProcessor {
	GENERATED_BODY()
private:
	virtual void Activate() override;

	virtual void Deactivate() override;

	virtual void Tick(const float InDeltaTime) override;

	UFUNCTION(CallInEditor)
	void Capture();

	void OnActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh);

	void OnRefreshLightActors();

	void OnRefreshCapturePlane();
private:
	UPROPERTY(EditAnywhere)
	TArray<TObjectPtr<AActor>> SourceActors;

	UPROPERTY(EditAnywhere)
	TArray<TObjectPtr<AActor>> LightActors;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<ASceneCapture2D> CaptureActor;

	UPROPERTY(EditAnywhere, meta = (UIMin = 0.01, ClampMin = 0.01, UIMax = 1, ClampMax = 1))
	float CaptureScale = 1.0f;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextureRenderTarget2D> CaptureRT;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTexture2D> BillboardTexture;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<AStaticMeshActor> BillboardCapturePlane;

	UPROPERTY(EditAnywhere, meta = (UIMin = 0.01, ClampMin = 0.01, UIMax = 2, ClampMax = 2))
	FVector2D BillboardCapturePlaneScale = FVector2D(1.0f, 1.0f);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMesh> CaptureResult;

	UPROPERTY(EditAnywhere, Config)
	FSoftObjectPath BillboardMaterial;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UMaterialInstanceDynamic> BillboardMaterialMID;

	FDelegateHandle OnActorSelectionChangedHandle;

	FBoxSphereBounds CurrentBounds;
	FBox2D CurrentScreenBounds;
};