#pragma once

#include "WorldPartition/HLOD/Builders/HLODBuilderMeshApproximate.h"
#include "HLODBuilderMeshApproximateEx.generated.h"

UCLASS()
class UHLODBuilderMeshApproximateEx : public UHLODBuilderMeshApproximate
{
	GENERATED_BODY()
public:
	virtual TArray<UActorComponent*> Build(const FHLODBuildContext& InHLODBuildContext, const TArray<UActorComponent*>& InSourceComponents) const override;

};
