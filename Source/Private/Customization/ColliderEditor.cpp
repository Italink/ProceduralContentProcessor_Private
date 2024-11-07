#include "ColliderEditor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Engine/StaticMeshActor.h"
#include "FileHelpers.h"
#include "GeometryProcessingInterfaces/ApproximateActors.h"
#include "IGeometryProcessingInterfacesModule.h"
#include "IMeshMergeUtilities.h"
#include "KismetProceduralMeshLibrary.h"
#include "MeshDescription.h"
#include "MeshMergeModule.h"
#include "MeshMergeModule.h"
#include "ObjectTools.h"
#include "PhysicsEngine/BodySetup.h"
#include "ProceduralMeshComponent.h"
#include "ProceduralMeshConversion.h"
#include "StaticMeshEditorSubsystem.h"
#include "StaticMeshEditorSubsystemHelpers.h"
#include "Engine/StaticMeshActor.h"
#include "LevelEditor.h"
#include "Selection.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceConstant.h"
#include "EditorSupportDelegates.h"
#include "MaterialEditingLibrary.h"

AStaticMeshActor* UCollisionMeshGenerateMethod_Proxy::Generate(TArray<AActor*> InActors)
{
	const IMeshMergeUtilities& MeshMergeUtilities = FModuleManager::Get().LoadModuleChecked<IMeshMergeModule>("MeshMergeUtilities").GetUtilities();
	
	FMeshMergingSettings MergeSettings;
	MergeSettings.LODSelectionType = EMeshLODSelectionType::LowestDetailLOD;
	MergeSettings.bMergeMeshSockets = false;
	MergeSettings.bMergeMaterials = bMergeMaterials;
	MergeSettings.bAllowDistanceField = false;
	MergeSettings.bGenerateLightMapUV = false;
	MergeSettings.bComputedLightMapResolution = false;
	MergeSettings.bMergePhysicsData = true;
	MergeSettings.bUseVertexDataForBakingMaterial = false;
	MergeSettings.bReuseMeshLightmapUVs = false;
	MergeSettings.bMergeEquivalentMaterials = false;
	MergeSettings.bIncludeImposters = false;

	FMeshProxySettings ProxySettings;
	ProxySettings.ScreenSize = ScreenSize;
	ProxySettings.VoxelSize = VoxelSize;
	ProxySettings.MergeDistance = MergeDistance;
	ProxySettings.bCalculateCorrectLODModel = bCalculateCorrectLODModel;
	ProxySettings.bOverrideVoxelSize = bOverrideVoxelSize;
	ProxySettings.bCreateCollision = true;
	ProxySettings.bReuseMeshLightmapUVs = false;
	ProxySettings.bSupportRayTracing = false;
	ProxySettings.bRecalculateNormals = false;
	ProxySettings.bUseLandscapeCulling = false;
	ProxySettings.bSupportRayTracing = false;
	ProxySettings.bAllowDistanceField = false;
	ProxySettings.bAllowVertexColors = false;
	ProxySettings.bGenerateLightmapUVs = false;
	ProxySettings.MaterialSettings.MaterialMergeType = bMergeMaterials ? EMaterialMergeType::MaterialMergeType_Simplygon : EMaterialMergeType::MaterialMergeType_Default;

	TArray<UPrimitiveComponent*> SourceCompList;
	UWorld* World = nullptr;
	for (auto Actor : InActors) {
		if (Actor == nullptr || Actor->HasAnyFlags(RF_Transient))
			continue;
		World = World ? World : Actor->GetWorld();
		for (auto Comp : Actor->GetComponents()) {
			if (auto StaticMeshComp = Cast<UStaticMeshComponent>(Comp)) {
				SourceCompList.AddUnique(StaticMeshComp);
				if (bRemoveSourceMeshCollision) {
					UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();
					StaticMeshEditorSubsystem->RemoveCollisionsWithNotification(StaticMeshComp->GetStaticMesh(), true);
				}
			}
		}
	}
	AStaticMeshActor* StaticMeshActor = nullptr;
	UStaticMesh* MergedStaticMesh = nullptr;
	UStaticMesh* ProxyedStaticMesh = nullptr;
	FVector MergedLocation;

	{
		TArray<UObject*> AssetsToSync;
		MeshMergeUtilities.MergeComponentsToStaticMesh(SourceCompList, nullptr, MergeSettings, nullptr, GetTransientPackage(), "MergedStaticMesh", AssetsToSync, MergedLocation, TNumericLimits<float>::Max(), true);
		if (AssetsToSync.IsEmpty())
			return nullptr;
		for (auto Asset : AssetsToSync) {
			if (auto StaticMesh = Cast<UStaticMesh>(Asset)) {
				MergedStaticMesh = StaticMesh;
			}
			else {
				Asset->MarkAsGarbage();
			}
		}
	}

	StaticMeshActor = World->SpawnActor<AStaticMeshActor>(MergedLocation, FRotator());

	if (bMergeMaterials) {
		FCreateProxyDelegate ProxyDelegate;
		ProxyDelegate.BindLambda([&ProxyedStaticMesh](const FGuid Guid, TArray<UObject*>& InAssetsToSync) {
			if (InAssetsToSync.Num()) {
				for (auto Asset : InAssetsToSync) {
					if (auto StaticMesh = Cast<UStaticMesh>(Asset)) {
						ProxyedStaticMesh = StaticMesh;
						for (int i = 0; i < StaticMesh->GetStaticMaterials().Num(); i++) {
							StaticMesh->SetMaterial(i, nullptr);
						}
					}
					else {
						//ObjectTools::ForceDeleteObjects({ Asset });
						Asset->MarkAsGarbage();
					}
				}
			}
			else {
				UE_LOG(LogTemp, Warning, TEXT("Mesh Is Invaild"));
			}
		});
		for (int i = 0; i < MergedStaticMesh->GetStaticMaterials().Num(); i++) {
			MergedStaticMesh->SetMaterial(i, nullptr);
		}
		FGuid JobGuid = FGuid::NewGuid();
		UStaticMeshComponent* StaticMeshComp = NewObject<UStaticMeshComponent>();
		StaticMeshComp->SetStaticMesh(MergedStaticMesh);
		MeshMergeUtilities.CreateProxyMesh({ StaticMeshComp }, ProxySettings, StaticMeshActor->GetPackage(), "ProxyStaticMesh", JobGuid, ProxyDelegate);
	}
	else {
		int SubMeshCount = MergedStaticMesh->GetNumSections(0);
		TArray<TPair<UStaticMesh*, UStaticMesh*>> SubMeshMap;
		for (int i = 0; i < SubMeshCount; i++) {
			TArray<FVector> Vertices;
			TArray<int32> Triangles;
			TArray<FVector> Normals;
			TArray<FVector2D> UVs;
			TArray<FProcMeshTangent> Tangents;
			UKismetProceduralMeshLibrary::GetSectionFromStaticMesh(MergedStaticMesh, 0, i, Vertices, Triangles, Normals, UVs, Tangents);
			UProceduralMeshComponent* ProceduralMeshComp = NewObject<UProceduralMeshComponent>();
			ProceduralMeshComp->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, TArray<FLinearColor>(), Tangents, true);
			FMeshDescription MeshDescription = BuildMeshDescription(ProceduralMeshComp);
			UStaticMesh* SubMesh = NewObject<UStaticMesh>();
			SubMesh->InitResources();
			SubMesh->SetLightingGuid();
			FStaticMeshSourceModel& SrcModel = SubMesh->AddSourceModel();
			SrcModel.BuildSettings.bRecomputeNormals = false;
			SrcModel.BuildSettings.bRecomputeTangents = false;
			SrcModel.BuildSettings.bRemoveDegenerates = false;
			SrcModel.BuildSettings.bUseHighPrecisionTangentBasis = false;
			SrcModel.BuildSettings.bUseFullPrecisionUVs = false;
			SrcModel.BuildSettings.bGenerateLightmapUVs = true;
			SrcModel.BuildSettings.SrcLightmapIndex = 0;
			SrcModel.BuildSettings.DstLightmapIndex = 1;
			SubMesh->CreateMeshDescription(0, MoveTemp(MeshDescription));
			SubMesh->CommitMeshDescription(0);

			if (!ProceduralMeshComp->bUseComplexAsSimpleCollision)
			{
				SubMesh->CreateBodySetup();
				UBodySetup* NewBodySetup = SubMesh->GetBodySetup();
				NewBodySetup->BodySetupGuid = FGuid::NewGuid();
				NewBodySetup->AggGeom.ConvexElems = ProceduralMeshComp->ProcMeshBodySetup->AggGeom.ConvexElems;
				NewBodySetup->bGenerateMirroredCollision = false;
				NewBodySetup->bDoubleSidedGeometry = true;
				NewBodySetup->CollisionTraceFlag = CTF_UseDefault;
				NewBodySetup->CreatePhysicsMeshes();
			}

			TSet<UMaterialInterface*> UniqueMaterials;
			const int32 NumSections = ProceduralMeshComp->GetNumSections();
			for (int32 SectionIdx = 0; SectionIdx < NumSections; SectionIdx++)
			{
				FProcMeshSection* ProcSection =
					ProceduralMeshComp->GetProcMeshSection(SectionIdx);
				UMaterialInterface* Material = ProceduralMeshComp->GetMaterial(SectionIdx);
				UniqueMaterials.Add(Material);
			}

			for (auto* Material : UniqueMaterials)
			{
				SubMesh->GetStaticMaterials().Add(FStaticMaterial(Material));
			}

			SubMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;

			SubMesh->Build(false);
			SubMesh->PostEditChange();
			SubMesh->AddToRoot();
			SubMeshMap.Add({ SubMesh, nullptr });

			FCreateProxyDelegate ProxyDelegate;
			ProxyDelegate.BindLambda(
				[SubMesh, &SubMeshMap, MergedStaticMesh, i](const FGuid Guid, TArray<UObject*>& InAssetsToSync)
				{
					if (InAssetsToSync.Num())
					{
						int32 AssetCount = InAssetsToSync.Num();
						for (int32 AssetIndex = AssetCount - 1; AssetIndex >= 0; AssetIndex--)
						{
							UStaticMesh* OutputStaticMesh = Cast<UStaticMesh>(InAssetsToSync[AssetIndex]);
							if (OutputStaticMesh) {
								OutputStaticMesh->GetStaticMaterials().Empty();
								OutputStaticMesh->GetStaticMaterials().Add(MergedStaticMesh->GetStaticMaterials()[MergedStaticMesh->GetSectionInfoMap().Get(0, i).MaterialIndex]);;
								for (auto& Pair : SubMeshMap) {
									if (Pair.Key == SubMesh) {
										Pair.Value = OutputStaticMesh;
										break;
									}
								}
							}
							else {
								ObjectTools::ForceDeleteObjects({ InAssetsToSync[AssetIndex] });
							}
						}
					}
					else {
						UE_LOG(LogTemp, Warning, TEXT("%s Submesh Is Invaild: SectionIndex[%d] MaterailIndex[%d]"), *MergedStaticMesh->GetPathName(), i, MergedStaticMesh->GetSectionInfoMap().Get(0, i).MaterialIndex);
					}
				});
			FGuid JobGuid = FGuid::NewGuid();
			UStaticMeshComponent* StaticMeshComp = NewObject<UStaticMeshComponent>(SubMesh);
			StaticMeshComp->SetStaticMesh(SubMesh);
			MeshMergeUtilities.CreateProxyMesh({ StaticMeshComp }, ProxySettings, GetTransientPackage(), SubMesh->GetName(), JobGuid, ProxyDelegate);
		}
		TArray<UPrimitiveComponent*> StaticMeshCompList;
		TMap<UStaticMesh*, int> MaterialIndexMap;
		TArray<UStaticMesh*> SubArrays;
		for (int i = 0; i < SubMeshMap.Num(); i++) {
			MaterialIndexMap.Add(SubMeshMap[i].Value, MergedStaticMesh->GetSectionInfoMap().Get(0, i).MaterialIndex);
			SubArrays.Add(SubMeshMap[i].Value);
		}

		SubArrays.Sort([&MaterialIndexMap](const UStaticMesh& a, const UStaticMesh& b) {
			return MaterialIndexMap[&a] < MaterialIndexMap[&b];
		});
		for (auto SubMesh : SubArrays) {
			if (SubMesh) {
				UStaticMeshComponent* StaticMeshComp = NewObject<UStaticMeshComponent>();
				StaticMeshComp->SetStaticMesh(SubMesh);
				StaticMeshCompList.Add(StaticMeshComp);
			}
		}
		TArray<UObject*> AssetsToSync;
		FVector Location;
		MeshMergeUtilities.MergeComponentsToStaticMesh(StaticMeshCompList, nullptr, MergeSettings, nullptr, StaticMeshActor->GetPackage() , "ProxyStaticMesh", AssetsToSync, Location, TNumericLimits<float>::Max(), true);
		for (auto& SubMesh : SubMeshMap) {
			SubMesh.Key->RemoveFromRoot();
			if (SubMesh.Value) {
				SubMesh.Value->MarkAsGarbage();
			}
		}
		if (AssetsToSync.IsEmpty())
			return nullptr;
		ProxyedStaticMesh = Cast<UStaticMesh>(AssetsToSync[0]);
		for (auto& SM : ProxyedStaticMesh->GetStaticMaterials()) {
			SM.MaterialInterface = nullptr;
		}
		ProxyedStaticMesh->CreateBodySetup();
	}

	StaticMeshActor->GetStaticMeshComponent()->SetStaticMesh(ProxyedStaticMesh);
	return StaticMeshActor;
}

void UColliderEditor::Activate()
{
	FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	OnActorSelectionChangedHandle = LevelEditor.OnActorSelectionChanged().AddUObject(this, &UColliderEditor::OnActorSelectionChanged);
	TArray<UObject*> Objects;
	GEditor->GetSelectedActors()->GetSelectedObjects(Objects);
	OnActorSelectionChanged(Objects, true);
	if (!DebugMaterial.IsValid()) {
		DebugMaterial = LoadObject<UMaterialInstanceConstant>(nullptr, TEXT("/ProceduralContentProcessor/WorldProcessor/Collider/MI_CollisionMesh.MI_CollisionMesh"));
	}
	if(auto DebugMaterialObject = Cast<UMaterialInstanceConstant>(DebugMaterial.TryLoad())){
		DebugMaterialObject->SetScalarParameterValueEditorOnly(FName("Opacity"), 0.5);;
		UMaterialEditingLibrary::UpdateMaterialInstance(DebugMaterialObject);
	}
}

void UColliderEditor::Deactivate()
{
	if (OnActorSelectionChangedHandle.IsValid()) {
		FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
		LevelEditor.OnActorSelectionChanged().Remove(OnActorSelectionChangedHandle);
		OnActorSelectionChangedHandle.Reset();
	}
	if (auto DebugMaterialObject = Cast<UMaterialInstanceConstant>(DebugMaterial.TryLoad())){
		DebugMaterialObject->SetScalarParameterValueEditorOnly(FName("Opacity"), 0);
		UMaterialEditingLibrary::UpdateMaterialInstance(DebugMaterialObject);
	}
}

void UColliderEditor::OnActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh)
{
	SourceActors.Reset();
	SourceMeshes.Reset();
	for (auto Item : NewSelection) {
		AActor* Actor = Cast<AActor>(Item);
		if (Actor == nullptr || Actor->HasAnyFlags(RF_Transient) || Actor->GetFolderPath().ToString().StartsWith("Collision"))
			continue;
		bool bHasMesh = false;
		for (auto Comp : Actor->GetComponents()) {
			if (auto StaticMeshComp = Cast<UStaticMeshComponent>(Comp)) {
				SourceMeshes.AddUnique(StaticMeshComp->GetStaticMesh());
				bHasMesh = true;
			}
		}
		if (bHasMesh) {
			SourceActors.AddUnique(Actor);
		}
	}
	RefreshColliderFinder();
}

void UColliderEditor::Generate()
{
	if(SourceActors.IsEmpty() || SourceMeshes.IsEmpty() || GenerateMethod == nullptr)
		return;
	AStaticMeshActor* Collider = GenerateMethod->Generate(SourceActors);
	GEditor->ForceGarbageCollection(true);
	Collider->Modify();
	Collider->SetActorHiddenInGame(true);
	Collider->SetFolderPath("Collision");
	for (auto Actor : SourceActors) {
		Collider->Tags.AddUnique(*Actor->GetActorGuid().ToString());
	}
	UStaticMesh* ColliderMesh = Collider->GetStaticMeshComponent()->GetStaticMesh();
	if (ColliderMesh) {
		if (auto DebugMaterialObject = Cast<UMaterialInstanceConstant>(DebugMaterial.TryLoad())) {
			for (int i = 0; i < ColliderMesh->GetStaticMaterials().Num(); i++) {
				ColliderMesh->SetMaterial(i, DebugMaterialObject);
			}
		}
		ColliderMesh->bCustomizedCollision = true;
		ColliderMesh->ComplexCollisionMesh = ColliderMesh;
		ColliderMesh->GetBodySetup()->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseComplexAsSimple;
		VertexCount = ColliderMesh->GetNumVertices(0);
		TriangleCount = ColliderMesh->GetNumTriangles(0);
	}
	GEditor->SelectActor(Collider, true, true);
	RefreshColliderFinder();
}

void UColliderEditor::RefreshColliderFinder()
{
	UWorld* World = GEditor->GetEditorWorldContext().World();
	ColliderFinder.Reset();
	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(World, AStaticMeshActor::StaticClass(), Actors);
	for (auto Actor : Actors) {
		if (Actor->GetFolderPath().ToString().StartsWith("Collision")) {
			for (auto Tag : Actor->Tags) {
				ColliderFinder.Add(FGuid(Tag.ToString()), Actor);
			}
		}
	}
}

AStaticMeshActor* UCollisionMeshGenerateMethod_Approximate::Generate(TArray<AActor*> InActors)
{
	return nullptr;
}
