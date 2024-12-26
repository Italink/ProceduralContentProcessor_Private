#pragma once

#include "WorldPartition/HLOD/HLODBuilder.h"
#include "HLODBuilderImpostor.generated.h"


UCLASS(Blueprintable)
class UHLODBuilderImpostorSettings : public UHLODBuilderSettings
{
	GENERATED_UCLASS_BODY()

	virtual uint32 GetCRC() const override;
};


UCLASS()
class UHLODBuilderImpostor : public UHLODBuilder
{
	GENERATED_UCLASS_BODY()
public:
	//virtual TSubclassOf<UHLODBuilderSettings> GetSettingsClass() const override;
	virtual TArray<UActorComponent*> Build(const FHLODBuildContext& InHLODBuildContext, const TArray<UActorComponent*>& InSourceComponents) const override;

	UPROPERTY(EditAnywhere, Config)
	TSubclassOf<UObject> BP_Generate_ImposterSprites;

	UPROPERTY(Transient)
	TObjectPtr<AActor> BP_Generate_ImposterSpritesActor;
};
