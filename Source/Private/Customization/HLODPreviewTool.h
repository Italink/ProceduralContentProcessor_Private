#pragma once

#include "ProceduralContentProcessor.h"
#include "HLODPreviewTool.generated.h"

class SHLODOutliner;

struct FWorldPartitionTextureStats {
	FString Path;
	int MemorySize;
	FIntPoint TextureSize;
};

struct FWorldPartitionActorStats {
	FTopLevelAssetPath BaseClass;
	FTopLevelAssetPath NativeClass;
	FSoftObjectPath Path;
	FName Package;
	FString Label;
	FGuid ActorGuid;

	int DrawCallCount;
	int TriangleCount;
	int TextureCount;
};

struct FWorldPartitionHlodStats {
	int Triangles;
	int TextureCount;
	int DrawCalls;
};

struct FWorldPartitionCellStats{
	FName CellName;
	FName CellPackage;
	FBox Bounds;
	int HierarchicalLevel;
	int Priority;
	bool bIsSpatiallyLoaded;
	TArray<FName> DataLayers;
	TArray<FWorldPartitionActorStats> Actors;
	FWorldPartitionHlodStats HLOD;

	int DrawCallCount;
	int TriangleCount;
	TMap<FString, int> ComponentCount;
	TArray<FSoftObjectPath> UsedTextures;
};

struct FWorldPartitionGridStats{
	FName GridName;
	FBox Bounds;
	int32 CellSize;
	int32 LoadingRange;
	TArray<FWorldPartitionCellStats> Cells;
};

struct FWorldPartitionStats{
	TArray<FWorldPartitionGridStats> Grids;
	int TotalTriangles;
};

UCLASS(EditInlineNew, CollapseCategories, config = ProceduralContentProcessor, defaultconfig, Category = "WorldPartition", meta = (DisplayName = "HLOD Preview Tool"))
class PROCEDURALCONTENTPROCESSOR_API UHLODPreviewTool: public UProceduralWorldProcessor {
	GENERATED_BODY()
protected:
	virtual TSharedPtr<SWidget> BuildWidget() override;
	FWorldPartitionStats Generate(UWorld* InWorld);
private:
	TSharedPtr<SHLODOutliner> HLODOutliner;
};