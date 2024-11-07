#pragma once

#include "ProceduralContentProcessor.h"
#include "ColliderEditor.generated.h"


UCLASS(Abstract, EditInlineNew, CollapseCategories, Blueprintable, BlueprintType, config = ProceduralContentProcessor, defaultconfig )
class PROCEDURALCONTENTPROCESSOR_API UCollisionMeshGenerateMethodBase : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Config)
	bool bMergeMaterials = true;

	UPROPERTY(EditAnywhere, Config)
	bool bRemoveSourceMeshCollision = true;

	virtual AStaticMeshActor* Generate(TArray<AActor*> InActors) { return nullptr; };
};

UCLASS(meta = (DisplayName = "Approximate"), CollapseCategories)
class PROCEDURALCONTENTPROCESSOR_API UCollisionMeshGenerateMethod_Approximate : public UCollisionMeshGenerateMethodBase
{
	GENERATED_BODY()
public:
	virtual AStaticMeshActor* Generate(TArray<AActor*> InActors) override;

	UPROPERTY(EditAnywhere, Config)
	FMeshApproximationSettings ApproximationSettings;
};

UCLASS(meta = (DisplayName = "Proxy"), CollapseCategories)
class PROCEDURALCONTENTPROCESSOR_API UCollisionMeshGenerateMethod_Proxy : public UCollisionMeshGenerateMethodBase
{
	GENERATED_BODY()
public:
	virtual AStaticMeshActor* Generate(TArray<AActor*> InActors) override;

	UPROPERTY( EditAnywhere, Config, meta = (ClampMin = "1", ClampMax = "1200", UIMin = "1", UIMax = "1200"))
	int32 ScreenSize = 50;

	/** Override when converting multiple meshes for proxy LOD merging. Warning, large geometry with small sampling has very high memory costs*/
	UPROPERTY(EditAnywhere, Config, meta = (EditCondition = "bOverrideVoxelSize", ClampMin = "0.1", DisplayName = "Override Spatial Sampling Distance"))
	float VoxelSize = 3.f;

	UPROPERTY(EditAnywhere, Config)
	float MergeDistance = 0;

	UPROPERTY(EditAnywhere, Config, meta = (DisplayAfter = "ScreenSize"))
	bool bCalculateCorrectLODModel = 0;

	/** If true, Spatial Sampling Distance will not be automatically computed based on geometry and you must set it directly */
	UPROPERTY(EditAnywhere, Config, AdvancedDisplay, meta = (InlineEditConditionToggle))
	bool bOverrideVoxelSize = false;
};

UCLASS(EditInlineNew, CollapseCategories, config = ProceduralContentProcessor, defaultconfig, Category = "WorldPartition", meta = (DisplayName = "Collider Editor"))
class UColliderEditor : public UProceduralWorldProcessor {
	GENERATED_BODY()
protected:
	virtual void Activate() override;
	virtual void Deactivate() override;
	void OnActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh);

	UFUNCTION(CallInEditor)
	void Generate();

	void RefreshColliderFinder();
private:
	UPROPERTY(EditAnywhere)
	TArray<TObjectPtr<AActor>> SourceActors;

	UPROPERTY(EditAnywhere)
	TArray<TObjectPtr<UStaticMesh>> SourceMeshes;

	UPROPERTY(EditAnywhere, Instanced, NoClear, meta = (ShowOnlyInnerProperties))
	UCollisionMeshGenerateMethodBase* GenerateMethod = NewObject<UCollisionMeshGenerateMethod_Proxy>();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<AStaticMeshActor> ColliderActor;

	UPROPERTY(EditAnywhere, Config)
	FSoftObjectPath DebugMaterial;

	UPROPERTY(VisibleAnywhere)
	int VertexCount;

	UPROPERTY(VisibleAnywhere)
	int TriangleCount;

	FDelegateHandle OnActorSelectionChangedHandle;

	TMap<FGuid, TObjectPtr<AActor>> ColliderFinder;
};

