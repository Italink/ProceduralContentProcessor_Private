#pragma once

#include "ProceduralContentProcessor.h"
#include "Landscape.h"
#include "LandscapePartitionTool.generated.h"

UCLASS(EditInlineNew, CollapseCategories, config = ProceduralContentProcessor, defaultconfig, Category = "WorldPartition")
class PROCEDURALCONTENTPROCESSOR_API ULandscapePartitionTool: public UProceduralWorldProcessor {
	GENERATED_BODY()
protected:
	virtual void Activate() override;

	virtual void Deactivate() override;

	UFUNCTION(BlueprintCallable, CallInEditor)
	void Repartition();

	FDelegateHandle OnActorSelectionChangedHandle;
	void OnActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh);

	UFUNCTION(BlueprintCallable, CallInEditor)
	void Export();

	UFUNCTION(BlueprintCallable, CallInEditor)
	void Import();
private:
	UPROPERTY(EditAnywhere)
	TObjectPtr<ALandscape> LandscapeActor;

	UPROPERTY(EditAnywhere, Config)
	int LandscapeGridSize = 16;

	UPROPERTY(EditAnywhere, Config)
	int ComponentNumSubSections = 16;
	
	UPROPERTY(EditAnywhere, Config)
	int SubsectionSizeQuads = 16;

	UPROPERTY(EditAnywhere, Config)
	int GridSize = 16;
};