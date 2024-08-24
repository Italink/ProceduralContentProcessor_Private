#include "ProceduralContentProcessor.h"
#include "LevelEditor.h"
#include "Kismet/GameplayStatics.h"
#include "ProceduralContentProcessorAssetActions.h"
#include "Engine/StaticMeshActor.h"
#include "InstancedFoliageActor.h"
#include "Layers/LayersSubsystem.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "Selection.h"
#include "DataLayer/DataLayerEditorSubsystem.h"
#include "AssetRegistry/AssetRegistryHelpers.h"
#include "GameFramework/ActorPrimitiveColorHandler.h"
#include "SLevelViewport.h"

#define LOCTEXT_NAMESPACE "ProceduralContentProcessor"

void UProceduralContentProcessor::Activate()
{
	ReceiveActivate();
}

void UProceduralContentProcessor::Tick(const float InDeltaTime)
{
	ReceiveTick();
}

void UProceduralContentProcessor::Deactivate()
{
	ReceiveDeactivate();
}

TSharedPtr<SWidget> UProceduralContentProcessor::BuildWidget()
{
	if (UProceduralContentProcessorBlueprint* BP = Cast<UProceduralContentProcessorBlueprint>(GetClass()->ClassGeneratedBy)) {
		if (BP->OverrideUMGClass) {
			if(!BP->OverrideUMG)
				BP->OverrideUMG = NewObject<UUserWidget>(GetTransientPackage(), BP->OverrideUMGClass);
			return BP->OverrideUMG->TakeWidget();
		}
	}

	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	//DetailsViewArgs.bAllowMultipleTopLevelObjects = true;
	DetailsViewArgs.bShowObjectLabel = false;
	DetailsViewArgs.bAllowSearch = true;
	DetailsViewArgs.bAllowFavoriteSystem = true;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::ENameAreaSettings::HideNameArea;
	DetailsViewArgs.ViewIdentifier = FName("BlueprintDefaults");
	auto DetailView = EditModule.CreateDetailView(DetailsViewArgs);
	DetailView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateLambda([](const FPropertyAndParent& Node) {
		return !Node.Property.HasAnyPropertyFlags(EPropertyFlags::CPF_AdvancedDisplay);
	}));
	DetailView->SetIsPropertyReadOnlyDelegate(FIsPropertyReadOnly::CreateLambda([](const FPropertyAndParent& Node) {
		return Node.Property.HasAnyPropertyFlags(EPropertyFlags::CPF_DisableEditOnInstance);
	}));
	DetailView->SetObject(this);
	return DetailView;
}

TScriptInterface<IAssetRegistry> UProceduralAssetProcessor::GetAssetRegistry()
{
	UClass* Class = LoadObject<UClass>(nullptr, TEXT("/Script/AssetRegistry.AssetRegistryHelpers"));
	UFunction* Func = Class->FindFunctionByName("GetAssetRegistry");
	TScriptInterface<IAssetRegistry> ReturnParam;
	FFrame Frame(Class->GetDefaultObject(), Func, nullptr, 0, Func->ChildProperties);
	Func->Invoke(Class->GetDefaultObject(), Frame, &ReturnParam);
	return ReturnParam;
}

TArray<AActor*> UProceduralWorldProcessor::GetLoadedActorsByClass(TSubclassOf<AActor> ActorClass)
{
	TArray<AActor*> OutActors;
	if (!GEditor)
		return OutActors;
	UWorld* World = GEditor->GetEditorWorldContext().World();
	UGameplayStatics::GetAllActorsOfClass(World, ActorClass, OutActors);
	return OutActors;
}

TArray<AActor*> UProceduralWorldProcessor::GetLoadedActorsByName(FString InName, bool bCompleteMatching /*= false*/)
{
	TArray<AActor*> OutActors;
	if (!GEditor)
		return OutActors;
	UWorld* World = GEditor->GetEditorWorldContext().World();
	UWorldPartition* WorldPartition = World->GetWorldPartition();
	TArray<AActor*> AllActors;
	UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);
	if (bCompleteMatching) {
		for (auto Actor : AllActors) {
			if (Actor->GetActorNameOrLabel() == InName) {
				OutActors.Add(Actor);
			}
		}
	}
	else {
		for (auto Actor : AllActors) {
			if (Actor->GetActorNameOrLabel().Contains(InName)) {
				OutActors.Add(Actor);
			}
		}
	}
	return OutActors;
}

TArray<AStaticMeshActor*> UProceduralWorldProcessor::GetLoadedStaticMeshActorsByAsset(TArray<UStaticMesh*> InMeshs)
{
	TArray<AStaticMeshActor*> OutActors;
	if (!GEditor)
		return OutActors;
	UWorld* World = GEditor->GetEditorWorldContext().World();
	UWorldPartition* WorldPartition = World->GetWorldPartition();
	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(World, AStaticMeshActor::StaticClass(), Actors);
	for (auto Actor : Actors) {
		if (AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>(Actor)) {
			if (InMeshs.Contains(StaticMeshActor->GetStaticMeshComponent()->GetStaticMesh())) {
				OutActors.Add(StaticMeshActor);
			}
		}
	}
	return OutActors;
}

TArray<UStaticMesh*> UProceduralWorldProcessor::GetLoadedStaticMesh(bool bContainStaticMesh/*= true*/, bool bContainInstancedStaticMesh /*= true*/)
{
	TArray<UStaticMesh*> StaticMesh;
	auto Actors = GetLoadedActorsByClass(AActor::StaticClass());
	for (auto Actor : Actors) {
		if (bContainStaticMesh) {
			TArray<UStaticMeshComponent*> StaicMeshComps;
			Actor->GetComponents(StaicMeshComps, true);
			for (auto StaticMeshComp : StaicMeshComps) {
				if (StaticMeshComp->GetStaticMesh()) {
					StaticMesh.AddUnique(StaticMeshComp->GetStaticMesh());
				}
			}
		}
		if (bContainInstancedStaticMesh) {
			TArray<UInstancedStaticMeshComponent*> InstanceComps;
			Actor->GetComponents(InstanceComps, true);
			for (auto InstStaticMeshComp : InstanceComps) {
				if (InstStaticMeshComp->GetStaticMesh()) {
					StaticMesh.AddUnique(InstStaticMeshComp->GetStaticMesh());
				}
			}
		}
	}
	return StaticMesh;
}

void UProceduralWorldProcessor::DisableInstancedFoliageMeshShadow(TArray<UStaticMesh*> InMeshes)
{
	if (!InMeshes.IsEmpty()) {
		GEditor->BeginTransaction(LOCTEXT("DisableInstancedFoliageMeshShadow", "Disable Instanced Foliage Mesh Shadow"));
		auto Actors = GetLoadedActorsByClass(AInstancedFoliageActor::StaticClass());
		for (auto Actor : Actors) {
			TArray<UInstancedStaticMeshComponent*> InstanceComps;
			Actor->GetComponents(InstanceComps, true);
			for (auto InstComp : InstanceComps) {
				if (InMeshes.Contains(InstComp->GetStaticMesh())) {
					InstComp->Modify();
					InstComp->bCastDynamicShadow = false;
					InstComp->bCastContactShadow = true;	
				}
			}
		}
		GEditor->EndTransaction();
	}
}

void UProceduralWorldProcessor::BreakInstancedStaticMeshComp(AActor* InSourceActor, bool bDestorySourceActor)
{
	if(!InSourceActor)
		return;
	ULayersSubsystem* LayersSubsystem = GEditor->GetEditorSubsystem<ULayersSubsystem>();
	TArray<UInstancedStaticMeshComponent*> InstanceComps;
	InSourceActor->GetComponents(InstanceComps, true);
	if (!InstanceComps.IsEmpty()) {
		for (auto& ISMC : InstanceComps) {
			for (auto PISMData : ISMC->PerInstanceSMData) {
				FActorSpawnParameters SpawnInfo;
				SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				auto NewActor = InSourceActor->GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(PISMData.Transform), SpawnInfo);
				NewActor->GetStaticMeshComponent()->SetStaticMesh(ISMC->GetStaticMesh());
				NewActor->SetActorLabel(InSourceActor->GetActorLabel());
				NewActor->Modify();
				LayersSubsystem->InitializeNewActorLayers(NewActor);
				const bool bCurrentActorSelected = GUnrealEd->GetSelectedActors()->IsSelected(InSourceActor);
				if (bCurrentActorSelected)
				{
					// The source actor was selected, so deselect the old actor and select the new one.
					GUnrealEd->GetSelectedActors()->Modify();
					GUnrealEd->SelectActor(NewActor, bCurrentActorSelected, false);
					GUnrealEd->SelectActor(InSourceActor, false, false);
				}
				{
					LayersSubsystem->DisassociateActorFromLayers(NewActor);
					NewActor->Layers.Empty();
					LayersSubsystem->AddActorToLayers(NewActor, InSourceActor->Layers);
				}

			}
			ISMC->PerInstanceSMData.Reset();
			if (bDestorySourceActor) {
				LayersSubsystem->DisassociateActorFromLayers(InSourceActor);
				InSourceActor->GetWorld()->EditorDestroyActor(InSourceActor, true);
			}
			GUnrealEd->NoteSelectionChange();
		}
	}
}

void UProceduralWorldProcessor::SetHLODLayer(AActor* InActor, UHLODLayer* InHLODLayer)
{
	if (InActor->GetHLODLayer() != InHLODLayer) {
		InActor->Modify();
		InActor->bEnableAutoLODGeneration = InHLODLayer != nullptr;
		InActor->SetHLODLayer(InHLODLayer);
	}
}

void UProceduralWorldProcessor::SelectActor(AActor* InActor)
{
	GEditor->GetSelectedActors()->Modify();
	GEditor->GetSelectedActors()->BeginBatchSelectOperation();
	GEditor->SelectNone(false, true, true);
	GEditor->SelectActor(InActor, true, false, true);
	GEditor->GetSelectedActors()->EndBatchSelectOperation(/*bNotify*/false);
	GEditor->NoteSelectionChange();
}

void UProceduralWorldProcessor::LookAtActor(AActor* InActor)
{
	GEditor->MoveViewportCamerasToActor(*InActor, true);
}

AActor* UProceduralWorldProcessor::ReplaceActor(AActor* InSrc, TSubclassOf<AActor> InDst, bool bNoteSelectionChange /*= false*/)
{
	if (!InSrc || !InDst)
		return nullptr;
	GEditor->BeginTransaction(LOCTEXT("ReplaceActor", "Replace Actor"));
	FTransform Transform = InSrc->GetTransform();
	auto NewActor = GUnrealEd->ReplaceActor(InSrc, InDst, nullptr, bNoteSelectionChange);
	NewActor->SetActorTransform(Transform);
	GEditor->EndTransaction();
	return NewActor;
}

void UProceduralWorldProcessor::ReplaceActors(TMap<AActor*, TSubclassOf<AActor>> ActorMap, bool bNoteSelectionChange)
{
	if (GUnrealEd && !ActorMap.IsEmpty()) {
		GEditor->BeginTransaction(LOCTEXT("ReplaceActors", "Replace Actors"));
		for (auto ActorPair : ActorMap) {
			if (ActorPair.Key && ActorPair.Value) {
				FTransform Transform = ActorPair.Key->GetTransform();
				auto NewActor = GUnrealEd->ReplaceActor(ActorPair.Key, ActorPair.Value, nullptr, bNoteSelectionChange);
				NewActor->SetActorTransform(Transform);
			}
		}
		GEditor->EndTransaction();
	}
}

void UProceduralWorldProcessor::ActorSetIsSpatiallyLoaded(AActor* Actor, bool bIsSpatiallyLoaded)
{
	Actor->SetIsSpatiallyLoaded(bIsSpatiallyLoaded);
}

bool UProceduralWorldProcessor::ActorAddDataLayer(AActor* Actor, UDataLayerAsset* DataLayerAsset)
{
	if (Actor && DataLayerAsset) {
		UDataLayerInstance* Instance = GEditor->GetEditorSubsystem<UDataLayerEditorSubsystem>()->GetDataLayerInstance(DataLayerAsset);
		if (Instance) {
			return Instance->AddActor(Actor);
		}
	}
	return false;
}

bool UProceduralWorldProcessor::ActorRemoveDataLayer(AActor* Actor, UDataLayerAsset* DataLayerAsset)
{
	if (Actor && DataLayerAsset) {
		UDataLayerInstance* Instance = GEditor->GetEditorSubsystem<UDataLayerEditorSubsystem>()->GetDataLayerInstance(DataLayerAsset);
		if (Instance) {
			return Instance->RemoveActor(Actor);
		}
	}
	return false;
}

FName UProceduralWorldProcessor::ActorGetRuntimeGrid(AActor* Actor)
{
	if (Actor) {
		return Actor->GetRuntimeGrid();
	}
	return FName();
}

void UProceduralWorldProcessor::ActorSetRuntimeGrid(AActor* Actor, FName GridName)
{
	if (Actor && Actor->GetRuntimeGrid() != GridName) {
		Actor->SetRuntimeGrid(GridName);
		Actor->Modify();
	}
}

UWorld* UProceduralWorldProcessor::GetWorld() const
{
	return GEditor->GetEditorWorldContext().World();
}

void UProceduralActorColorationProcessor::Activate()
{
	Super::Activate();
	auto GetColorFunc = [this](const UPrimitiveComponent* PrimitiveComponent){
		return this->Colour(PrimitiveComponent);
	};
	FActorPrimitiveColorHandler::Get().RegisterPrimitiveColorHandler(*GetClass()->GetDisplayNameText().ToString(), GetClass()->GetDisplayNameText(), GetColorFunc);
	FActorPrimitiveColorHandler::Get().SetActivePrimitiveColorHandler(*GetClass()->GetDisplayNameText().ToString(),GetWorld());

	FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	SLevelViewport* LevelViewport = LevelEditor.GetFirstActiveLevelViewport().Get();
	FLevelEditorViewportClient& LevelViewportClient = LevelViewport->GetLevelViewportClient();
	LevelViewportClient.EngineShowFlags.SetSingleFlag(FEngineShowFlags::SF_ActorColoration, true);
	LevelViewportClient.Invalidate();

	//GEditor->OnLevelActorListChanged().AddUObject(this,&UProceduralActorColorationProcessor::OnLevelActorListChanged);
	//GEditor->OnLevelActorAdded().AddUObject(this, &UProceduralActorColorationProcessor::OnLevelActorAdded);
	//GEditor->OnLevelActorDeleted().AddUObject(this, &UProceduralActorColorationProcessor::OnLevelActorRemoved);
}

void UProceduralActorColorationProcessor::Deactivate()
{
	Super::Deactivate();
	FActorPrimitiveColorHandler::Get().UnregisterPrimitiveColorHandler(*GetClass()->GetDisplayNameText().ToString());

	FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	if (SLevelViewport* LevelViewport = LevelEditor.GetFirstActiveLevelViewport().Get()) {
		FLevelEditorViewportClient& LevelViewportClient = LevelViewport->GetLevelViewportClient();
		LevelViewportClient.EngineShowFlags.SetSingleFlag(FEngineShowFlags::SF_ActorColoration, false);
		LevelViewportClient.Invalidate();
	}
	//GEditor->OnLevelActorListChanged().RemoveAll(this);
	//GEngine->OnLevelActorAdded().RemoveAll(this);
	//GEngine->OnLevelActorDeleted().RemoveAll(this);
}

void UProceduralActorColorationProcessor::OnLevelActorListChanged()
{
	//FActorPrimitiveColorHandler::Get().RefreshPrimitiveColorHandler(*GetClass()->GetDisplayNameText().ToString(), GetWorld());
}

void UProceduralActorColorationProcessor::OnLevelActorAdded(AActor* InActor)
{
	//FActorPrimitiveColorHandler::Get().RefreshPrimitiveColorHandler(*GetClass()->GetDisplayNameText().ToString(), GetWorld());
}

void UProceduralActorColorationProcessor::OnLevelActorRemoved(AActor* InActor)
{
	//FActorPrimitiveColorHandler::Get().RefreshPrimitiveColorHandler(*GetClass()->GetDisplayNameText().ToString(), GetWorld());
}

FLinearColor UProceduralActorColorationProcessor::Colour_Implementation(const UPrimitiveComponent* PrimitiveComponent)
{
	return FLinearColor::White;
}

#undef LOCTEXT_NAMESPACE

