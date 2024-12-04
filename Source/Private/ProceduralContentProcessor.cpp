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
#include "SLevelViewport.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Materials/MaterialInstanceConstant.h"
#include "StaticMeshAttributes.h"
#include "Engine/TextureRenderTarget.h"
#include "ProceduralMeshComponent.h"
#include "ProceduralMeshConversion.h"
#include "StaticMeshCompiler.h"
#include "Engine/TextureRenderTarget2D.h"
#include "PropertyCustomizationHelpers.h"

#if ENGINE_MAJOR_VERSION >=5 && ENGINE_MINOR_VERSION >= 4
#include "GameFramework/ActorPrimitiveColorHandler.h"
#endif

#define LOCTEXT_NAMESPACE "ProceduralContentProcessor"

void UProceduralContentProcessor::Activate()
{
	ReceiveActivate();
}

void UProceduralContentProcessor::Tick(const float InDeltaTime)
{
	ReceiveTick(InDeltaTime);
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
		return !(Node.Property.HasAnyPropertyFlags(EPropertyFlags::CPF_Transient) || Node.Property.HasAnyPropertyFlags(EPropertyFlags::CPF_DisableEditOnInstance));
	}));
	DetailView->SetIsPropertyReadOnlyDelegate(FIsPropertyReadOnly::CreateLambda([](const FPropertyAndParent& Node) {
		return Node.Property.HasAnyPropertyFlags(EPropertyFlags::CPF_DisableEditOnInstance) || Node.Property.HasAnyPropertyFlags(EPropertyFlags::CPF_BlueprintReadOnly);
	}));
	DetailView->SetObject(this);
	return DetailView;
}

TSharedPtr<SWidget> UProceduralContentProcessor::BuildToolBar()
{
	return SNullWidget::NullWidget;
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

TArray<AActor*> UProceduralWorldProcessor::GetAllActorsByName(FString InName, bool bCompleteMatching /*= false*/)
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

void UProceduralWorldProcessor::DisableInstancedFoliageMeshShadow(TArray<UStaticMesh*> InMeshes)
{
	if (!InMeshes.IsEmpty()) {
		GEditor->BeginTransaction(LOCTEXT("DisableInstancedFoliageMeshShadow", "Disable Instanced Foliage Mesh Shadow"));
		TArray<AActor*> Actors;
		UWorld* World = GEditor->GetEditorWorldContext().World();
		UGameplayStatics::GetAllActorsOfClass(World, AInstancedFoliageActor::StaticClass(), Actors);
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

TArray<AActor*> UProceduralWorldProcessor::BreakISM(AActor* InISMActor, bool bDestorySourceActor /*= true*/)
{
	TArray<AActor*> Actors;
	if (!InISMActor)
		return Actors;
	ULayersSubsystem* LayersSubsystem = GEditor->GetEditorSubsystem<ULayersSubsystem>();
	TArray<UInstancedStaticMeshComponent*> InstanceComps;
	InISMActor->GetComponents(InstanceComps, true);
	UWorld* World = InISMActor->GetWorld();
	if (!InstanceComps.IsEmpty()) {
		for (auto& ISMC : InstanceComps) {
			for(int i = 0; i< ISMC->GetInstanceCount(); i++){
				FActorSpawnParameters SpawnInfo;
				SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				FTransform Transform;
				ISMC->GetInstanceTransform(i, Transform,true);
				auto NewActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Transform, SpawnInfo);
				NewActor->GetStaticMeshComponent()->SetStaticMesh(ISMC->GetStaticMesh());
				NewActor->SetActorLabel(InISMActor->GetActorLabel());
				auto Materials = ISMC->GetMaterials();
				for (int j = 0; j < Materials.Num(); j++) 
					NewActor->GetStaticMeshComponent()->SetMaterial(j, Materials[j]);
				NewActor->Modify();
				NewActor->SetActorLabel(MakeUniqueObjectName(World, AStaticMeshActor::StaticClass(), *ISMC->GetStaticMesh()->GetName()).ToString());
				Actors.Add(NewActor);
				LayersSubsystem->InitializeNewActorLayers(NewActor);
				const bool bCurrentActorSelected = GUnrealEd->GetSelectedActors()->IsSelected(InISMActor);
				if (bCurrentActorSelected)
				{
					// The source actor was selected, so deselect the old actor and select the new one.
					GUnrealEd->GetSelectedActors()->Modify();
					GUnrealEd->SelectActor(NewActor, bCurrentActorSelected, false);
					GUnrealEd->SelectActor(InISMActor, false, false);
				}
				{
					LayersSubsystem->DisassociateActorFromLayers(NewActor);
					NewActor->Layers.Empty();
					LayersSubsystem->AddActorToLayers(NewActor, InISMActor->Layers);
				}
			}
			ISMC->PerInstanceSMData.Reset();
			if (bDestorySourceActor) {
				LayersSubsystem->DisassociateActorFromLayers(InISMActor);
				InISMActor->GetWorld()->EditorDestroyActor(InISMActor, true);
			}
			GUnrealEd->NoteSelectionChange();
		}
	}
	return Actors;
}

AActor* UProceduralWorldProcessor::MergeISM(TArray<AActor*> InSourceActors, TSubclassOf<UInstancedStaticMeshComponent> InISMClass, bool bDestorySourceActor /*= true*/)
{
	if (InSourceActors.IsEmpty() || !InISMClass)
		return nullptr;
	ULayersSubsystem* LayersSubsystem = GEditor->GetEditorSubsystem<ULayersSubsystem>();
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	UWorld* World = InSourceActors[0]->GetWorld();
	FBoxSphereBounds Bounds;
	TMap<UStaticMesh*, TArray<FTransform>> InstancedMap;
	for (auto Actor : InSourceActors) {
		TArray<UStaticMeshComponent*> MeshComps;
		Actor->GetComponents(MeshComps, true);
		for (auto MeshComp : MeshComps) {
			UStaticMesh* Mesh = MeshComp->GetStaticMesh();
			auto& InstancedInfo = InstancedMap.FindOrAdd(Mesh);
			Bounds = MeshComp->Bounds + Bounds;
			InstancedInfo.Add(MeshComp->K2_GetComponentToWorld());
		}
		TArray<UInstancedStaticMeshComponent*> InstMeshComps;
		Actor->GetComponents(InstMeshComps, true);
		for (auto InstMeshComp : InstMeshComps) {
			UStaticMesh* Mesh = InstMeshComp->GetStaticMesh();
			auto& InstancedInfo = InstancedMap.FindOrAdd(Mesh);
			FTransform Transform;
			Bounds = InstMeshComp->Bounds + Bounds;
			for(int i = 0 ;i< InstMeshComp->GetInstanceCount();i++){
				InstMeshComp->GetInstanceTransform(i, Transform, true);
				InstancedInfo.Add(Transform);
			}
		}
	}
	FTransform Transform;
	Transform.SetLocation(Bounds.Origin);
	auto NewISMActor = World->SpawnActor<AActor>(AActor::StaticClass(),Transform, SpawnInfo);
	USceneComponent* RootComponent = NewObject<USceneComponent>(NewISMActor, USceneComponent::GetDefaultSceneRootVariableName(), RF_Transactional);
	RootComponent->Mobility = EComponentMobility::Static;
	RootComponent->bVisualizeComponent = true;
	NewISMActor->SetRootComponent(RootComponent);
	NewISMActor->AddInstanceComponent(RootComponent);
	RootComponent->OnComponentCreated();
	RootComponent->RegisterComponent();

	for (const auto& InstancedInfo : InstancedMap) {
		UInstancedStaticMeshComponent* ISMComponent = NewObject<UInstancedStaticMeshComponent>(NewISMActor, InISMClass, *InstancedInfo.Key->GetName(), RF_Transactional);
		ISMComponent->Mobility = EComponentMobility::Static;
		NewISMActor->AddInstanceComponent(ISMComponent);
		ISMComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
		ISMComponent->OnComponentCreated();
		ISMComponent->RegisterComponent();
		ISMComponent->SetStaticMesh(InstancedInfo.Key);
		for (auto InstanceTransform : InstancedInfo.Value) {
			ISMComponent->AddInstance(InstanceTransform, true);
		}
	}
	NewISMActor->Modify();
	LayersSubsystem->InitializeNewActorLayers(NewISMActor);
	GUnrealEd->GetSelectedActors()->Modify();
	GUnrealEd->SelectActor(NewISMActor, true, false);
	if (bDestorySourceActor) {
		for (auto Actor : InSourceActors) {
			LayersSubsystem->DisassociateActorFromLayers(Actor);
			World->EditorDestroyActor(Actor, true);
		}
	}
	return NewISMActor;
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

AActor* UProceduralWorldProcessor::SpawnTransientActor(UObject* WorldContextObject, TSubclassOf<AActor> Class, FTransform Transform)
{
	if(WorldContextObject == nullptr)
		return nullptr;
	UWorld* World = WorldContextObject->GetWorld();
	AActor* Actor = World->SpawnActor(Class, &Transform);
	if(Actor)
		Actor->SetFlags(RF_Transient);
	return Actor;
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

bool UProceduralWorldProcessor::ActorContainsDataLayer(AActor* Actor, UDataLayerAsset* DataLayerAsset)
{
	if (Actor && DataLayerAsset) {
		UDataLayerInstance* Instance = GEditor->GetEditorSubsystem<UDataLayerEditorSubsystem>()->GetDataLayerInstance(DataLayerAsset);
		if (Instance) {
			return Actor->ContainsDataLayer(Instance);
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
	if (GEditor && GEditor->PlayWorld != nullptr) {
		return GEditor->PlayWorld;
	}
	return GEditor->GetEditorWorldContext().World();
}

void UProceduralActorColorationProcessor::Activate()
{
	Super::Activate();
	bVisible = true;
	RefreshVisibility();
}

void UProceduralActorColorationProcessor::Deactivate()
{
	Super::Deactivate();
	bVisible = false;
	RefreshVisibility();
}

FReply UProceduralActorColorationProcessor::OnVisibilityClicked()
{
	bVisible = !bVisible;
	RefreshVisibility();
	return FReply::Handled();
}

void UProceduralActorColorationProcessor::RefreshVisibility()
{
	if (bVisible) {
#if ENGINE_MAJOR_VERSION >=5 && ENGINE_MINOR_VERSION >= 4
		auto GetColorFunc = [this](const UPrimitiveComponent* PrimitiveComponent) {
			return this->Colour(PrimitiveComponent);
		};
		FActorPrimitiveColorHandler::Get().RegisterPrimitiveColorHandler(*GetClass()->GetDisplayNameText().ToString(), GetClass()->GetDisplayNameText(), GetColorFunc);
		FActorPrimitiveColorHandler::Get().SetActivePrimitiveColorHandler(*GetClass()->GetDisplayNameText().ToString(), GetWorld());

		FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
		SLevelViewport* LevelViewport = LevelEditor.GetFirstActiveLevelViewport().Get();
		FLevelEditorViewportClient& LevelViewportClient = LevelViewport->GetLevelViewportClient();
		LevelViewportClient.EngineShowFlags.SetSingleFlag(FEngineShowFlags::SF_ActorColoration, true);
		LevelViewportClient.Invalidate();
#endif
	}
	else {
#if ENGINE_MAJOR_VERSION >=5 && ENGINE_MINOR_VERSION >= 4
		FActorPrimitiveColorHandler::Get().UnregisterPrimitiveColorHandler(*GetClass()->GetDisplayNameText().ToString());

		FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
		if (SLevelViewport* LevelViewport = LevelEditor.GetFirstActiveLevelViewport().Get()) {
			FLevelEditorViewportClient& LevelViewportClient = LevelViewport->GetLevelViewportClient();
			LevelViewportClient.EngineShowFlags.SetSingleFlag(FEngineShowFlags::SF_ActorColoration, false);
			LevelViewportClient.Invalidate();
		}
#endif
	}
}

TSharedPtr<SWidget> UProceduralActorColorationProcessor::BuildToolBar()
{
	return PropertyCustomizationHelpers::MakeVisibilityButton(
		FOnClicked::CreateUObject(this, &UProceduralActorColorationProcessor::OnVisibilityClicked),
		FText(),
		TAttribute<bool>::CreateLambda([this]() { return bVisible; })
	).ToSharedPtr();
}

FLinearColor UProceduralActorColorationProcessor::Colour_Implementation(const UPrimitiveComponent* PrimitiveComponent)
{
	return FLinearColor::White;
}

#undef LOCTEXT_NAMESPACE

