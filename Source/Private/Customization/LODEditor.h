#pragma once

#include "ProceduralContentProcessor.h"
#include "Engine/StaticMeshActor.h"
#include "LODEditor.generated.h"

UENUM(BlueprintType)
enum class EStaticMeshLODGenerateType : uint8
{
	Reduce,
	Imposter,
};

USTRUCT(BlueprintType)
struct FMeshImposterSettings {
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int Resolution = 2048;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Config)
	int FrameXY = 16;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Config, meta = (AllowedClasses = "/Script/Engine.MaterialInterface") )
	FSoftObjectPath ParentMaterial;
};

USTRUCT(BlueprintType)
struct FStaticMeshChainNode
{
	GENERATED_BODY()
public:

	UPROPERTY(BlueprintReadWrite)
	bool bUseDistance = true;

	UPROPERTY(BlueprintReadWrite)
	bool bEnableBuildSetting = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditConditionHides, HideEditConditionToggle, EditCondition = "!bUseDistance"))
	float ScreenSize = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (HideEditConditionToggle, EditCondition = "bUseDistance"))
	float Distance = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EStaticMeshLODGenerateType Type = EStaticMeshLODGenerateType::Reduce;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=BuildSettings, meta = (EditConditionHides, HideEditConditionToggle, EditCondition = "bEnableBuildSetting"))
	FMeshBuildSettings BuildSettings;

	UPROPERTY(EditAnywhere, meta = (EditConditionHides, HideEditConditionToggle, EditCondition = "Type==EStaticMeshLODGenerateType::Reduce"))
	FMeshReductionSettings ReductionSettings; 
};

UCLASS(EditInlineNew, CollapseCategories, config = ProceduralContentProcessor, defaultconfig, Category = "Model", DisplayName = "LOD Editor")
class PROCEDURALCONTENTPROCESSOR_API ULODEditor: public UProceduralWorldProcessor {
	GENERATED_BODY()
public:
	ULODEditor();

	UPROPERTY(EditAnywhere)
	TArray<TObjectPtr<UStaticMesh>> StaticMeshes;

	UPROPERTY(Config)
	TArray<FSoftObjectPath> StaticMeshesForConfig;

	UPROPERTY(EditAnywhere, Config)
	bool bUseDistance = true;

	UPROPERTY(EditAnywhere, Config)
	bool bEnableBuildSetting = false;

	UPROPERTY(EditAnywhere, Config)
	FPerPlatformInt MinLOD;

	UPROPERTY(EditAnywhere, Config)
	TSubclassOf<UObject> BP_Generate_ImposterSprites;

	UPROPERTY(EditAnywhere, Config)
	FMeshImposterSettings ImposterSettings;

	UPROPERTY(Transient)
	TObjectPtr<AActor> BP_Generate_ImposterSpritesActor;

	UPROPERTY(VisibleAnywhere, Transient)
	TObjectPtr<AStaticMeshActor> SelectedStaticMeshActor;

	UPROPERTY(EditAnywhere)
	TArray<FStaticMeshChainNode> SelectedStaticMeshLODChain;

	virtual void Activate() override;

	virtual void Deactivate() override;

	TArray<FStaticMeshChainNode> AutoEvaluateLodChain(AStaticMeshActor* MeshActor);

	UFUNCTION(BlueprintCallable, CallInEditor, meta = (DisplayPriority = 10, DisplayName = "Auto Evaluate For Selected"))
	void AutoEvaluateLodChainForSelected();

	UFUNCTION(BlueprintCallable, CallInEditor, meta = (DisplayPriority = 20, DisplayName = "Generate For Selected"))
	void GenerateLODForSelected();

	UFUNCTION(BlueprintCallable, CallInEditor, meta = (DisplayPriority = 30, DisplayName = "Auto Generate For All"))
	void GenerateLODForAll();

	UFUNCTION(BlueprintCallable, CallInEditor, meta = (DisplayPriority = 30, DisplayName = "Auto Generate For All"))
	void ApplyMinLODForAll();

	UFUNCTION(BlueprintCallable, CallInEditor, meta = (DisplayPriority = 40, DisplayName = "RefreshPreviewMatrix"))
	void RefreshPreviewMatrix();

	FDelegateHandle OnActorSelectionChangedHandle;
	void OnActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh);

	void RefreshLODChain();

	void ApplyImposterToLODChain(AStaticMeshActor* InStaticMeshActor, AActor* BP_Generate_ImposterSprites, int TargetLODIndex, float ScreenSize);

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
};