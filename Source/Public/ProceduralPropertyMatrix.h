#pragma once
#include "UObject/Object.h"
#include "ProceduralPropertyMatrix.generated.h"

struct FProceduralPropertyMatrixInfo {
	TWeakObjectPtr<UObject> Object;
	TArray<TPair<FName, FString>> Fields;
};

USTRUCT(BlueprintType)
struct FProceduralPropertyMatrix{
	GENERATED_BODY()
public:
	TMap<UObject*, TSharedPtr<FProceduralPropertyMatrixInfo>> ObjectInfoMap;

	TArray<TSharedPtr<FProceduralPropertyMatrixInfo>> ObjectInfoList;

	UPROPERTY()
	TArray<FName> FieldKeys;

	UPROPERTY()
	bool bIsDirty = false;

	TSharedPtr<SListView<TSharedPtr<FProceduralPropertyMatrixInfo>>> ObjectInfoListView;

	FName SortedColumnName;

	EColumnSortMode::Type SortMode = EColumnSortMode::None;
};