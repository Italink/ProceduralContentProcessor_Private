#pragma once

#include "ProceduralContentProcessor.h"
#include "FoliagePartitionTool.generated.h"

UCLASS(EditInlineNew, CollapseCategories, config = ProceduralContentProcessor, defaultconfig, Category = "WorldPartition")
class PROCEDURALCONTENTPROCESSOR_API UFoliagePartitionTool: public UProceduralWorldProcessor {
	GENERATED_BODY()
public:
protected:
	UPROPERTY(EditAnywhere, Config)
	int CellSize = 25600;

	UPROPERTY(EditAnywhere, Config)
	FIntPoint Origin;

	UPROPERTY(EditAnywhere)
	TArray<TObjectPtr<UStaticMesh>> StaticMeshes;

	UPROPERTY(Config)
	TArray<FSoftObjectPath> StaticMeshesForConfig;

	virtual void Activate() override;

	virtual void Deactivate() override;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	UFUNCTION(BlueprintCallable, CallInEditor)
	void ToggleFoliagePartition();

	UFUNCTION(BlueprintCallable, CallInEditor)
	void Fixup();

	UFUNCTION(BlueprintCallable, CallInEditor)
	void BreakAllHISM();
};

