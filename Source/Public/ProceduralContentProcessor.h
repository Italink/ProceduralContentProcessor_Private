#pragma once
#include "UObject/Object.h"
#include "GameFramework/Actor.h"
#include "Blueprint/UserWidget.h"
#include "ProceduralContentProcessor.generated.h"

UCLASS(Abstract, Blueprintable, EditInlineNew, CollapseCategories, config = ProceduralContentProcessor, defaultconfig)
class PROCEDURALCONTENTPROCESSOR_API UProceduralContentProcessor: public UObject {
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintImplementableEvent, Category = "ProceduralContentProcessor", meta = (DisplayName = "Activate"))
	void ReceiveActivate();

	UFUNCTION(BlueprintImplementableEvent, Category = "ProceduralContentProcessor", meta = (DisplayName = "Tick"))
	void ReceiveTick(float DeltaTime);

	UFUNCTION(BlueprintImplementableEvent, Category = "ProceduralContentProcessor", meta = (DisplayName = "Deactivate"))
	void ReceiveDeactivate();

	UFUNCTION(BlueprintImplementableEvent, Category = "ProceduralContentProcessor", meta = (DisplayName = "PostEditChangeProperty"))
	void ReceivePostEditChangeProperty(FName PropertyName);

	virtual void Activate();

	virtual void Tick(const float InDeltaTime);

	virtual void Deactivate();

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override{
		ReceivePostEditChangeProperty(PropertyChangedEvent.GetPropertyName());
		TryUpdateDefaultConfigFile();
	}

	virtual TSharedPtr<SWidget> BuildWidget();
};

UCLASS(Abstract, Blueprintable, EditInlineNew, CollapseCategories, config = ProceduralContentProcessor, defaultconfig)
class PROCEDURALCONTENTPROCESSOR_API UProceduralAssetProcessor: public UProceduralContentProcessor {
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category = "ProceduralContentProcessor")
	static TScriptInterface<IAssetRegistry> GetAssetRegistry();
};

UCLASS(Abstract, Blueprintable, EditInlineNew, CollapseCategories, config = ProceduralContentProcessor, defaultconfig)
class PROCEDURALCONTENTPROCESSOR_API UProceduralWorldProcessor: public UProceduralContentProcessor {
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static TArray<AActor*> GetAllActorsByName(FString InName, bool bCompleteMatching = false);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static void DisableInstancedFoliageMeshShadow(TArray<UStaticMesh*> InMeshes);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static TArray<AActor*> BreakISM(AActor* InISMActor, bool bDestorySourceActor = true);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static AActor* MergeISM(TArray<AActor*> InSourceActors, TSubclassOf<UInstancedStaticMeshComponent> InISMClass, bool bDestorySourceActor = true);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static void SetHLODLayer(AActor* InActor, UHLODLayer *InHLODLayer);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static void SelectActor(AActor* InActor);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static void LookAtActor(AActor* InActor);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static AActor* ReplaceActor(AActor* InSrc, TSubclassOf<AActor> InDst, bool bNoteSelectionChange = false);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static void ReplaceActors(TMap<AActor*, TSubclassOf<AActor>> ActorMap, bool bNoteSelectionChange);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static void ActorSetIsSpatiallyLoaded(AActor* Actor, bool bIsSpatiallyLoaded);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static bool ActorAddDataLayer(AActor* Actor, UDataLayerAsset* DataLayerAsset);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static bool ActorRemoveDataLayer(AActor* Actor, UDataLayerAsset* DataLayerAsset);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static bool ActorContainsDataLayer(AActor* Actor, UDataLayerAsset* DataLayerAsset);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static FName ActorGetRuntimeGrid(AActor* Actor);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static void ActorSetRuntimeGrid(AActor* Actor, FName GridName);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static bool HasImposter(AStaticMeshActor* InStaticMeshActor);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static void AppendImposterToLODChain(AStaticMeshActor* InStaticMeshActor, AActor* BP_Generate_ImposterSprites, float ScreenSize);
protected:
	virtual UWorld* GetWorld() const override;
};

UCLASS(Abstract, Blueprintable, EditInlineNew, CollapseCategories, config = ProceduralContentProcessor, defaultconfig)
class PROCEDURALCONTENTPROCESSOR_API UProceduralActorColorationProcessor: public UProceduralWorldProcessor {
	GENERATED_BODY()
public:
	virtual void Activate() override;
	virtual void Deactivate() override;

	UFUNCTION(BlueprintNativeEvent, Category = "ProceduralContentProcessor", meta = (DisplayName = "Colour"))
	FLinearColor Colour(const UPrimitiveComponent* PrimitiveComponent);

protected:
	void OnLevelActorListChanged();
	void OnLevelActorAdded(AActor* InActor);
	void OnLevelActorRemoved(AActor* InActor);
};