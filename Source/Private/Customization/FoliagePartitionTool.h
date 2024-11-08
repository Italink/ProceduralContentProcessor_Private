#pragma once

#include "ProceduralContentProcessor.h"
#include "FoliagePartitionTool.generated.h"

UCLASS(EditInlineNew, CollapseCategories, config = ProceduralContentProcessor, defaultconfig, Category = "WorldPartition")
class PROCEDURALCONTENTPROCESSOR_API UFoliagePartitionTool: public UProceduralWorldProcessor {
	GENERATED_BODY()
public:
protected:
	UFUNCTION(BlueprintCallable, CallInEditor)
	void ToggleFoliagePartition();

	UPROPERTY(EditAnywhere, Config)
	int CellSize = 25600;

	UPROPERTY(EditAnywhere, Config)
	FIntPoint Origin;

	UPROPERTY(EditAnywhere, Config)
	TArray<FSoftObjectPath> FoliageMeshes;
};