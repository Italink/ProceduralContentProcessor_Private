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

//bool UProceduralWorldProcessor::HasImposter(AStaticMeshActor* InStaticMeshActor)
//{
//	if (!InStaticMeshActor)
//		return false;
//	UStaticMesh* StaticMesh = InStaticMeshActor->GetStaticMeshComponent()->GetStaticMesh();
//	UMaterialInstanceConstant* MIC = FindObject<UMaterialInstanceConstant>(StaticMesh, TEXT("MIC_Impostor"));
//	return MIC != nullptr;
//}
//
//void UProceduralWorldProcessor::SetStaticMeshLODChain(AStaticMeshActor* InStaticMeshActor, AActor* BP_Generate_ImposterSprites, TArray<FStaticMeshChainNode> InChain)
//{
//	if (InStaticMeshActor == nullptr)
//		return;
//	UStaticMesh* StaticMesh = InStaticMeshActor->GetStaticMeshComponent()->GetStaticMesh();
//	if (StaticMesh == nullptr || StaticMesh->GetNumSourceModels() == 0 || InChain.IsEmpty())
//		return;
//	bool bStaticMeshIsEdited = false;
//	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
//	if (AssetEditorSubsystem->FindEditorForAsset(StaticMesh, false))
//	{
//		AssetEditorSubsystem->CloseAllEditorsForAsset(StaticMesh);
//		bStaticMeshIsEdited = true;
//	}
//
//	const float FOV = 60.0f;
//	const float FOVRad = FOV * (float)UE_PI / 360.0f;
//	const FMatrix ProjectionMatrix = FPerspectiveMatrix(FOVRad, 1920, 1080, 0.01f);
//
//	const float ScreenMultiple = FMath::Max(0.5f * ProjectionMatrix.M[0][0], 0.5f * ProjectionMatrix.M[1][1]);
//	const float SphereRadius = StaticMesh->GetBounds().SphereRadius;
//
//	StaticMesh->Modify();
//	StaticMesh->SetNumSourceModels(1);
//	StaticMesh->GetSourceModel(0).ReductionSettings = InChain[0].ReductionSettings;
//	StaticMesh->GetSourceModel(0).ScreenSize = InChain[0].ScreenSize;
//	if (InChain[0].bUseDistance) {
//		if (InChain[0].Distance == 0) {
//			StaticMesh->GetSourceModel(0).ScreenSize = 2.0f;
//		}
//		else {
//			StaticMesh->GetSourceModel(0).ScreenSize = 2.0f * ScreenMultiple * SphereRadius / FMath::Max(1.0f, InChain[0].Distance);
//		}
//	}
//
//	int32 LODIndex = 1;
//	for (; LODIndex < InChain.Num(); ++LODIndex) {
//		FStaticMeshSourceModel& SrcModel = StaticMesh->AddSourceModel();
//		const FStaticMeshChainNode& ChainNode = InChain[LODIndex];
//		float ScreenSize = InChain[LODIndex].ScreenSize;
//		if (InChain[LODIndex].bUseDistance) {
//			if (InChain[LODIndex].Distance == 0) {
//				ScreenSize = 2.0f;
//			}
//			else {
//				ScreenSize = 2.0f * ScreenMultiple * SphereRadius / FMath::Max(1.0f, InChain[LODIndex].Distance);
//			}
//		}
//		if (ChainNode.Type == EStaticMeshLODGenerateType::Reduce) {
//			SrcModel.BuildSettings = InChain[LODIndex].BuildSettings;
//			SrcModel.ReductionSettings = InChain[LODIndex].ReductionSettings;
//			SrcModel.ScreenSize = ScreenSize;
//		}
//		else {
//			ApplyImposterToLODChain(InStaticMeshActor, BP_Generate_ImposterSprites, LODIndex, ScreenSize, ChainNode.ImposterSettings);
//		}
//
//		// Stop when reaching maximum of supported LODs
//		if (StaticMesh->GetNumSourceModels() == MAX_STATIC_MESH_LODS) {
//			break;
//		}
//	}
//	StaticMesh->bAutoComputeLODScreenSize = 0;
//	StaticMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;
//	StaticMesh->Build();
//
//	StaticMesh->PostEditChange();
//	StaticMesh->MarkPackageDirty();
//	StaticMesh->WaitForPendingInitOrStreaming(true, true);
//
//	FStaticMeshCompilingManager::Get().FinishAllCompilation();
//
//	if (bStaticMeshIsEdited)
//	{
//		AssetEditorSubsystem->OpenEditorForAsset(StaticMesh);
//	}
//}
//
//void UProceduralWorldProcessor::ApplyImposterToLODChain(AStaticMeshActor* InStaticMeshActor, AActor* BP_Generate_ImposterSprites, int TargetLODIndex, float ScreenSize, FMeshImposterSettings ImposterSettings)
//{
//	if (!InStaticMeshActor || !BP_Generate_ImposterSprites)
//		return;
//	UStaticMesh* StaticMesh = InStaticMeshActor->GetStaticMeshComponent()->GetStaticMesh();
//	UClass* BPClass = BP_Generate_ImposterSprites->GetClass();
//	if (FIntProperty* ResolutionProp = FindFProperty<FIntProperty>(BPClass, "Resolution")) {
//		ResolutionProp->SetValue_InContainer(BP_Generate_ImposterSprites, ImposterSettings.Resolution);
//	}
//	if (FIntProperty* FrameXYProp = FindFProperty<FIntProperty>(BPClass, "Frame XY")) {
//		FrameXYProp->SetValue_InContainer(BP_Generate_ImposterSprites, ImposterSettings.Resolution);
//	}
//	int32 ImpostorType = -1;
//	UMaterialInterface* MaterialInterface = nullptr;
//	if (FByteProperty* ImpostorTypeProp = FindFProperty<FByteProperty>(BPClass, "Impostor Type")) {
//		ImpostorType = ImpostorTypeProp->GetUnsignedIntPropertyValue_InContainer(BP_Generate_ImposterSprites);
//	}
//	FObjectProperty* MaterialProp = nullptr;
//	if (ImpostorType == 0) {
//		MaterialProp = FindFProperty<FObjectProperty>(BPClass, "Full Sphere Material");
//	}
//	else if (ImpostorType == 1) {
//		MaterialProp = FindFProperty<FObjectProperty>(BPClass, "Upper Hemisphere Material");
//	}
//	else if (ImpostorType == 2) {
//		MaterialProp = FindFProperty<FObjectProperty>(BPClass, "Billboard Material");
//	}
//	if (MaterialProp) {
//		if (ImposterSettings.ParentMaterial) {
//			MaterialProp->SetObjectPropertyValue_InContainer(BP_Generate_ImposterSprites, ImposterSettings.ParentMaterial);
//			MaterialInterface = ImposterSettings.ParentMaterial;
//		}
//		else {
//			MaterialInterface = Cast<UMaterialInterface>(MaterialProp->GetObjectPropertyValue_InContainer(BP_Generate_ImposterSprites));
//		}
//	}
//
//	if (UFunction* ClearRTFunc = BPClass->FindFunctionByName("1) Clear RTs")) {
//		BP_Generate_ImposterSprites->ProcessEvent(ClearRTFunc, nullptr);
//	}
//	if (UFunction* RenderFramesFunc = BPClass->FindFunctionByName("2) RenderFrames")) {
//		if (FObjectProperty* ActorProp = FindFProperty<FObjectProperty>(BPClass, "Static Mesh Actor")) {
//			ActorProp->SetObjectPropertyValue_InContainer(BP_Generate_ImposterSprites, InStaticMeshActor);
//			BP_Generate_ImposterSprites->UserConstructionScript();
//		}
//		BP_Generate_ImposterSprites->ProcessEvent(RenderFramesFunc, nullptr);
//	}
//	bool bNeedNewMesh = false;
//	int32 MaterialIndex = 0;
//	UMaterialInstanceConstant* MIC = FindObject<UMaterialInstanceConstant>(StaticMesh, TEXT("MIC_Impostor"));
//	auto MICS = StaticMesh->GetStaticMaterials();
//	if (MIC != nullptr) {
//		if (StaticMesh->GetStaticMaterials().Contains(MIC)) {
//			const auto& LastTriangles = StaticMesh->GetNumTriangles(StaticMesh->GetNumSourceModels() - 1);
//			if (LastTriangles != 8) {
//				MaterialIndex = StaticMesh->GetStaticMaterials().IndexOfByKey(MIC);
//				bNeedNewMesh = true;
//			}
//		}
//		else {
//			MIC->ConditionalBeginDestroy();
//			MIC = nullptr;
//		}
//	}
//	if (MIC == nullptr) {
//		UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
//		Factory->InitialParent = MaterialInterface;
//		MIC = Cast<UMaterialInstanceConstant>(Factory->FactoryCreateNew(UMaterialInstanceConstant::StaticClass(), StaticMesh, "MIC_Impostor", RF_Standalone | RF_Public, NULL, GWarn));
//		MaterialIndex = StaticMesh->GetStaticMaterials().Add(FStaticMaterial(MIC));
//		bNeedNewMesh = true;
//	}
//	FProperty* TargetMapsProp = FindFProperty<FProperty>(BPClass, "TargetMaps");
//	TMap<uint32, UTextureRenderTarget*> TargetMaps;
//	TargetMapsProp->GetValue_InContainer(BP_Generate_ImposterSprites, &TargetMaps);
//	const uint32 BaseColorIndex = 0;
//	const uint32 NormalIndex = 6;
//	UTextureRenderTarget2D* TextureRenderTarget2D = Cast<UTextureRenderTarget2D>(TargetMaps[BaseColorIndex]);
//	UTexture* NewObj = TextureRenderTarget2D->ConstructTexture2D(MIC, "BaseColor", TextureRenderTarget2D->GetMaskedFlags() | RF_Public | RF_Standalone,
//		static_cast<EConstructTextureFlags>(CTF_Default | CTF_SkipPostEdit), /*InAlphaOverride = */nullptr);
//	NewObj->CompressionSettings = TextureCompressionSettings::TC_Default;
//	NewObj->MarkPackageDirty();
//	NewObj->PostEditChange();
//	MIC->SetTextureParameterValueEditorOnly(FName("BaseColor"), NewObj);
//
//	TextureRenderTarget2D = Cast<UTextureRenderTarget2D>(TargetMaps[NormalIndex]);
//	NewObj = TextureRenderTarget2D->ConstructTexture2D(MIC, "Normal", TextureRenderTarget2D->GetMaskedFlags() | RF_Public | RF_Standalone,
//		static_cast<EConstructTextureFlags>(CTF_Default | CTF_AllowMips | CTF_SkipPostEdit), /*InAlphaOverride = */nullptr);
//	NewObj->CompressionSettings = TextureCompressionSettings::TC_Default;
//	NewObj->MarkPackageDirty();
//	NewObj->PostEditChange();
//	MIC->SetTextureParameterValueEditorOnly(FName("Normal"), NewObj);
//
//	if (FIntProperty* Prop = FindFProperty<FIntProperty>(BPClass, "FramesXYInternal")) {
//		int FrameXY = Prop->GetSignedIntPropertyValue_InContainer(BP_Generate_ImposterSprites);
//		MIC->SetScalarParameterValueEditorOnly(FName("FramesXY"), FrameXY);
//	}
//	if (FNumericProperty* Prop = FindFProperty<FNumericProperty>(BPClass, "Object Radius")) {
//		double ObjectRadius = 0;
//		Prop->GetValue_InContainer(BP_Generate_ImposterSprites, &ObjectRadius);
//		MIC->SetScalarParameterValueEditorOnly(FName("Default Mesh Size"), ObjectRadius * 2);
//	}
//	if (FProperty* Prop = FindFProperty<FProperty>(BPClass, "Offset Vector")) {
//		FVector OffsetVector;
//		Prop->GetValue_InContainer(BP_Generate_ImposterSprites, &OffsetVector);
//		MIC->SetVectorParameterValueEditorOnly(FName("Pivot Offset"), FLinearColor(OffsetVector));
//	}
//
//	if (bNeedNewMesh) {
//		const int32 BaseLOD = 0;
//		FStaticMeshSourceModel* SourceModel = nullptr;
//		if (TargetLODIndex <= StaticMesh->GetNumSourceModels()) {
//			SourceModel = &StaticMesh->GetSourceModel(TargetLODIndex);
//		}
//		else {
//			int32 TargetLODIndex = StaticMesh->GetNumSourceModels();
//			SourceModel = &StaticMesh->AddSourceModel();
//		}
//		FMeshDescription& NewMeshDescription = *StaticMesh->CreateMeshDescription(TargetLODIndex);
//		FStaticMeshAttributes(NewMeshDescription).Register();
//
//		UProceduralMeshComponent* ProceduralMeshComp = BP_Generate_ImposterSprites->GetComponentByClass<UProceduralMeshComponent>();
//		NewMeshDescription = BuildMeshDescription(ProceduralMeshComp);
//
//		SourceModel->BuildSettings = StaticMesh->GetSourceModel(BaseLOD).BuildSettings;
//		SourceModel->BuildSettings.bUseHighPrecisionTangentBasis = false;
//		SourceModel->BuildSettings.bUseFullPrecisionUVs = false;
//		SourceModel->BuildSettings.bRecomputeNormals = false;
//		SourceModel->BuildSettings.bRecomputeTangents = false;
//		SourceModel->BuildSettings.bRemoveDegenerates = true;
//		SourceModel->BuildSettings.BuildScale3D = FVector(1, 1, 1);
//		SourceModel->ScreenSize.Default = ScreenSize;
//
//		StaticMesh->CommitMeshDescription(TargetLODIndex);
//		FMeshSectionInfo Info = StaticMesh->GetSectionInfoMap().Get(TargetLODIndex, 0);
//		Info.MaterialIndex = MaterialIndex;
//		StaticMesh->GetSectionInfoMap().Set(TargetLODIndex, 0, Info);
//	}
//}

UWorld* UProceduralWorldProcessor::GetWorld() const
{
	return GEditor->GetEditorWorldContext().World();
}

void UProceduralActorColorationProcessor::Activate()
{
	Super::Activate();
#if ENGINE_MAJOR_VERSION >=5 && ENGINE_MINOR_VERSION >= 4
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
#endif
	//GEditor->OnLevelActorListChanged().AddUObject(this,&UProceduralActorColorationProcessor::OnLevelActorListChanged);
	//GEditor->OnLevelActorAdded().AddUObject(this, &UProceduralActorColorationProcessor::OnLevelActorAdded);
	//GEditor->OnLevelActorDeleted().AddUObject(this, &UProceduralActorColorationProcessor::OnLevelActorRemoved);
}

void UProceduralActorColorationProcessor::Deactivate()
{
	Super::Deactivate();
#if ENGINE_MAJOR_VERSION >=5 && ENGINE_MINOR_VERSION >= 4
	FActorPrimitiveColorHandler::Get().UnregisterPrimitiveColorHandler(*GetClass()->GetDisplayNameText().ToString());

	FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	if (SLevelViewport* LevelViewport = LevelEditor.GetFirstActiveLevelViewport().Get()) {
		FLevelEditorViewportClient& LevelViewportClient = LevelViewport->GetLevelViewportClient();
		LevelViewportClient.EngineShowFlags.SetSingleFlag(FEngineShowFlags::SF_ActorColoration, false);
		LevelViewportClient.Invalidate();
	}
#endif
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

