#pragma once

#include "StaticMeshLODChain.generated.h"

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

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int FrameXY = 16;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TObjectPtr<UMaterialInterface> ParentMaterial;
};

USTRUCT(BlueprintType)
struct FStaticMeshChainNode
{
	GENERATED_BODY()
public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bUseDistance = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
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

	UPROPERTY(EditAnywhere, meta = (EditConditionHides, HideEditConditionToggle, EditCondition = "Type==EStaticMeshLODGenerateType::Imposter"))
	FMeshImposterSettings ImposterSettings;
};
