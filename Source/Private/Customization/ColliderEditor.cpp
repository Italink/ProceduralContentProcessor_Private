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

UStaticMesh* UCollisionMeshGenerateMethod_Proxy::Generate(UStaticMesh* InMesh)
{
	FMeshProxySettings ProxySettings;
	ProxySettings.ScreenSize = ScreenSize;
	ProxySettings.VoxelSize = VoxelSize;
	ProxySettings.MergeDistance = MergeDistance;
	ProxySettings.bCalculateCorrectLODModel = bCalculateCorrectLODModel;
	ProxySettings.bOverrideVoxelSize = bOverrideVoxelSize;
	ProxySettings.bCreateCollision = false;
	ProxySettings.bReuseMeshLightmapUVs = false;
	ProxySettings.bSupportRayTracing = false;
	ProxySettings.bRecalculateNormals = false;
	ProxySettings.bUseLandscapeCulling = false;
	ProxySettings.bSupportRayTracing = false;
	ProxySettings.bAllowDistanceField = false;
	ProxySettings.bAllowVertexColors = false;
	ProxySettings.bGenerateLightmapUVs = false;

	const IMeshMergeUtilities& MeshMergeUtilities = FModuleManager::Get().LoadModuleChecked<IMeshMergeModule>("MeshMergeUtilities").GetUtilities();
	int SubMeshCount = InMesh->GetNumSections(0);
	TArray<TPair<UStaticMesh*, UStaticMesh*>> SubMeshMap;
	for (int i = 0; i < SubMeshCount; i++) {
		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FProcMeshTangent> Tangents;
		UKismetProceduralMeshLibrary::GetSectionFromStaticMesh(InMesh, 0, i, Vertices, Triangles, Normals, UVs, Tangents);
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
			[SubMesh, &SubMeshMap, InMesh, i](const FGuid Guid, TArray<UObject*>& InAssetsToSync)
			{
				if (InAssetsToSync.Num())
				{
					int32 AssetCount = InAssetsToSync.Num();
					for (int32 AssetIndex = AssetCount - 1; AssetIndex >= 0; AssetIndex--)
					{
						UStaticMesh* OutputStaticMesh = Cast<UStaticMesh>(InAssetsToSync[AssetIndex]);
						if (OutputStaticMesh) {
							OutputStaticMesh->GetStaticMaterials().Empty();
							OutputStaticMesh->GetStaticMaterials().Add(InMesh->GetStaticMaterials()[InMesh->GetSectionInfoMap().Get(0, i).MaterialIndex]);;
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
					UE_LOG(LogTemp, Warning, TEXT("%s Submesh Is Invaild: SectionIndex[%d] MaterailIndex[%d]"), *InMesh->GetPathName(), i, InMesh->GetSectionInfoMap().Get(0, i).MaterialIndex);
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
		MaterialIndexMap.Add(SubMeshMap[i].Value, InMesh->GetSectionInfoMap().Get(0, i).MaterialIndex);
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
	FMeshMergingSettings MergeSettings;
	MergeSettings.bAllowDistanceField = false;
	MergeSettings.bGenerateLightMapUV = false;
	MergeSettings.bComputedLightMapResolution = false;
	MergeSettings.bMergePhysicsData = false;
	MergeSettings.bUseVertexDataForBakingMaterial = false;
	MergeSettings.bReuseMeshLightmapUVs = false;
	MergeSettings.bMergeMaterials = false;
	MergeSettings.bMergeEquivalentMaterials = false;
	MergeSettings.bIncludeImposters = false;
	TArray<UObject*> AssetsToSync;
	FVector Location;
	MeshMergeUtilities.MergeComponentsToStaticMesh(StaticMeshCompList, nullptr, MergeSettings, nullptr, InMesh->GetPackage(), "Collider", AssetsToSync, Location, TNumericLimits<float>::Max(), true);
	for (auto& SubMesh : SubMeshMap) {
		SubMesh.Key->RemoveFromRoot();
		if (SubMesh.Value) {
			ObjectTools::ForceDeleteObjects({ SubMesh.Value });
		}
	}
	if (AssetsToSync.IsEmpty())
		return nullptr;
	UStaticMesh* ResultMesh = Cast<UStaticMesh>(AssetsToSync[0]);
	for (auto& SM : ResultMesh->GetStaticMaterials()) {
		SM.MaterialInterface = nullptr;
	}
	FEditorFileUtils::PromptForCheckoutAndSave({ ResultMesh->GetPackage() }, false, false);
	return ResultMesh;
}

UStaticMesh* UCollisionMeshGenerateMethod_Approximate::Generate(UStaticMesh* InMesh)
{
	//FMeshApproximationSettings ApproximationSettings;
	//ApproximationSettings.OutputType = EMeshApproximationType::MeshShapeOnly;
	//ApproximationSettings.ApproximationAccuracy = ApproximationAccuracy;
	//ApproximationSettings.ClampVoxelDimension = ClampVoxelDimension;
	//ApproximationSettings.bAttemptAutoThickening = bAttemptAutoThickening;
	//ApproximationSettings.TargetMinThicknessMultiplier = TargetMinThicknessMultiplier;
	//ApproximationSettings.bIgnoreTinyParts = bIgnoreTinyParts;
	//ApproximationSettings.TinyPartSizeMultiplier = TinyPartSizeMultiplier;
	//ApproximationSettings.BaseCapping = BaseCapping;
	//ApproximationSettings.WindingThreshold = WindingThreshold;
	//ApproximationSettings.bFillGaps = bFillGaps;
	//ApproximationSettings.GapDistance = GapDistance;
	//ApproximationSettings.OcclusionMethod = OcclusionMethod;
	//ApproximationSettings.bOccludeFromBottom = bOccludeFromBottom;
	//ApproximationSettings.SimplifyMethod = SimplifyMethod;
	//ApproximationSettings.TargetTriCount = TargetTriCount;
	//ApproximationSettings.TrianglesPerM = TrianglesPerM;
	//ApproximationSettings.GeometricDeviation = GeometricDeviation;
	//ApproximationSettings.MultiSamplingAA = MultiSamplingAA;
	//ApproximationSettings.RenderCaptureResolution = RenderCaptureResolution;
	//ApproximationSettings.CaptureFieldOfView = CaptureFieldOfView;
	//ApproximationSettings.NearPlaneDist = NearPlaneDist;
	//ApproximationSettings.bUseRenderLODMeshes = bUseRenderLODMeshes;
	//ApproximationSettings.bEnableSimplifyPrePass = bEnableSimplifyPrePass;
	//ApproximationSettings.bEnableParallelBaking = bEnableParallelBaking;
	//ApproximationSettings.bSupportRayTracing = false;
	//ApproximationSettings.bAllowDistanceField = false;
	//ApproximationSettings.bOccludeFromBottom = false;

	IGeometryProcessingInterfacesModule& GeomProcInterfaces = FModuleManager::Get().LoadModuleChecked<IGeometryProcessingInterfacesModule>("GeometryProcessingInterfaces");
	IGeometryProcessing_ApproximateActors* ApproxActorsAPI = GeomProcInterfaces.GetApproximateActorsImplementation();
	ApproximationSettings.OutputType = EMeshApproximationType::MeshShapeOnly;

	IGeometryProcessing_ApproximateActors::FOptions Options = ApproxActorsAPI->ConstructOptions(ApproximationSettings);



	UWorld* TempWorld = UWorld::CreateWorld(EWorldType::Game, true);
	AStaticMeshActor* TempActor = TempWorld->SpawnActor<AStaticMeshActor>();

	const IMeshMergeUtilities& MeshMergeUtilities = FModuleManager::Get().LoadModuleChecked<IMeshMergeModule>("MeshMergeUtilities").GetUtilities();
	int SubMeshCount = InMesh->GetNumSections(0);
	TArray<TPair<UStaticMesh*, UStaticMesh*>> SubMeshMap;
	for (int i = 0; i < SubMeshCount; i++) {
		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FProcMeshTangent> Tangents;
		UKismetProceduralMeshLibrary::GetSectionFromStaticMesh(InMesh, 0, i, Vertices, Triangles, Normals, UVs, Tangents);
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
		TempActor->GetStaticMeshComponent()->SetStaticMesh(SubMesh);
		TArray<TObjectPtr<AActor>> Actors;
		Actors.Add(TempActor);
		IGeometryProcessing_ApproximateActors::FResults Results;
		ApproxActorsAPI->ApproximateActors({ Actors }, Options, Results);
		UStaticMesh* OutputStaticMesh;
		if (Results.NewMeshAssets.Num() != 0) {
			OutputStaticMesh = Cast<UStaticMesh>(Results.NewMeshAssets[0]);
			OutputStaticMesh->GetStaticMaterials().Empty();
			OutputStaticMesh->GetStaticMaterials().Add(InMesh->GetStaticMaterials()[InMesh->GetSectionInfoMap().Get(0, i).MaterialIndex]);;
			for (auto& Pair : SubMeshMap) {
				if (Pair.Key == SubMesh) {
					Pair.Value = OutputStaticMesh;
					break;
				}
			}
		}
	}
	TArray<UPrimitiveComponent*> StaticMeshCompList;

	TMap<UStaticMesh*, int> MaterialIndexMap;
	TArray<UStaticMesh*> SubArrays;
	for (int i = 0; i < SubMeshMap.Num(); i++) {
		MaterialIndexMap.Add(SubMeshMap[i].Value, InMesh->GetSectionInfoMap().Get(0, i).MaterialIndex);
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
	FMeshMergingSettings MergeSettings;
	MergeSettings.bAllowDistanceField = false;
	MergeSettings.bGenerateLightMapUV = false;
	MergeSettings.bComputedLightMapResolution = false;
	MergeSettings.bMergePhysicsData = false;
	MergeSettings.bUseVertexDataForBakingMaterial = false;
	MergeSettings.bReuseMeshLightmapUVs = false;
	MergeSettings.bMergeMaterials = false;
	MergeSettings.bMergeEquivalentMaterials = false;
	MergeSettings.bIncludeImposters = false;
	TArray<UObject*> AssetsToSync;
	FVector Location;
	MeshMergeUtilities.MergeComponentsToStaticMesh(StaticMeshCompList, nullptr, MergeSettings, nullptr, InMesh->GetPackage(), "Collider", AssetsToSync, Location, TNumericLimits<float>::Max(), true);
	//TempWorld->MarkObjectsPendingKill();
	if (AssetsToSync.IsEmpty())
		return nullptr;
	UStaticMesh* ResultMesh = Cast<UStaticMesh>(AssetsToSync[0]);
	FAssetRegistryModule& AssetRegistry = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	for (auto& SM : ResultMesh->GetStaticMaterials()) {
		SM.MaterialInterface = nullptr;
	}
	FEditorFileUtils::PromptForCheckoutAndSave({ ResultMesh->GetPackage() }, false, false);
	return ResultMesh;
}

void UColliderEditor::Activate()
{
	FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	OnActorSelectionChangedHandle = LevelEditor.OnActorSelectionChanged().AddUObject(this, &UColliderEditor::OnActorSelectionChanged);
}

void UColliderEditor::Deactivate()
{
	if (OnActorSelectionChangedHandle.IsValid()) {
		FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
		LevelEditor.OnActorSelectionChanged().Remove(OnActorSelectionChangedHandle);
		OnActorSelectionChangedHandle.Reset();
	}
	if (StaticMeshActor) {
		StaticMeshActor->Destroy();
	}
}

void UColliderEditor::OnActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh)
{
	if (NewSelection.Num() > 0) {
		AStaticMeshActor* Actor = Cast<AStaticMeshActor>(NewSelection[0]);
		if(Actor == nullptr || Actor->HasAnyFlags(RF_Transient))
			return;
		StaticMeshActor = Actor;
		if (StaticMeshActor) {
			if (ColliderActor == nullptr) {
				UWorld* World = StaticMeshActor->GetWorld();
				ColliderActor = World->SpawnActor<AStaticMeshActor>();
				ColliderActor->SetFlags(RF_Transient);
			}
			UStaticMesh* StaticMesh = StaticMeshActor->GetStaticMeshComponent()->GetStaticMesh();
			if (StaticMesh) {
				ColliderActor->GetStaticMeshComponent()->SetStaticMesh(StaticMesh->ComplexCollisionMesh);
				ColliderActor->SetActorTransform(StaticMeshActor->GetActorTransform());
				if (StaticMesh->ComplexCollisionMesh) {
					VertexCount = StaticMesh->ComplexCollisionMesh->GetNumVertices(0);
					TriangleCount = StaticMesh->ComplexCollisionMesh->GetNumTriangles(0);
					auto WireframeMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/ProceduralContentProcessor/WorldProcessor/Collider/M_Collider.M_Collider"));
					for (int i = 0; i < ColliderActor->GetStaticMeshComponent()->GetNumMaterials(); i++) {
						ColliderActor->GetStaticMeshComponent()->SetMaterial(i, LoadObject<UMaterialInterface>(nullptr, TEXT("/ProceduralContentProcessor/WorldProcessor/Collider/M_Collider.M_Collider")));
					}
				}
			}
		}
	}
}

void UColliderEditor::Generate()
{
	if(StaticMeshActor == nullptr || GenerateMethod == nullptr)
		return;
	UStaticMesh* StaticMesh = StaticMeshActor->GetStaticMeshComponent()->GetStaticMesh();
	if (StaticMesh == nullptr)
		return;
	UStaticMesh* Collider = GenerateMethod->Generate(StaticMesh);
	StaticMesh->ComplexCollisionMesh = Collider;
	StaticMesh->bCustomizedCollision = true;
	StaticMesh->GetBodySetup()->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseComplexAsSimple;
	StaticMesh->Modify();

	if (StaticMesh->ComplexCollisionMesh) {
		VertexCount = StaticMesh->ComplexCollisionMesh->GetNumVertices(0);
		TriangleCount = StaticMesh->ComplexCollisionMesh->GetNumTriangles(0);
	}
}
