#include "HLODBuilderMeshApproximateEx.h"
#include "IMeshMergeUtilities.h"
#include "MeshMergeModule.h"
#include "MeshMerge/MeshMergingSettings.h"
#include "Engine/StaticMeshActor.h"
#include "StaticMeshCompiler.h"
#include "AssetCompilingManager.h"
#include "Components/InstancedStaticMeshComponent.h"

TArray<UActorComponent*> UHLODBuilderMeshApproximateEx::Build(const FHLODBuildContext& InHLODBuildContext, const TArray<UActorComponent*>& InSourceComponents) const
{
	const IMeshMergeUtilities& MeshMergeUtilities = FModuleManager::Get().LoadModuleChecked<IMeshMergeModule>("MeshMergeUtilities").GetUtilities();

	const UHLODBuilderMeshApproximateSettings* MeshApproximateSettings = CastChecked<UHLODBuilderMeshApproximateSettings>(HLODBuilderSettings);

	FMeshMergingSettings MergeSettings;
	MergeSettings.MaterialSettings = MeshApproximateSettings->MeshApproximationSettings.MaterialSettings;
	MergeSettings.bAllowDistanceField = MeshApproximateSettings->MeshApproximationSettings.bAllowDistanceField;
	MergeSettings.LODSelectionType = EMeshLODSelectionType::SpecificLOD;
	MergeSettings.SpecificLOD = 0;
	MergeSettings.bMergeMeshSockets = true;
	MergeSettings.bMergeMaterials = true;
	MergeSettings.bGenerateLightMapUV = false;
	MergeSettings.bComputedLightMapResolution = false;
	MergeSettings.bMergePhysicsData = false;
	MergeSettings.bUseVertexDataForBakingMaterial = false;
	MergeSettings.bReuseMeshLightmapUVs = false;
	MergeSettings.bMergeEquivalentMaterials = true;
	MergeSettings.bIncludeImposters = false;
	MergeSettings.bUseTextureBinning = true;
	UWorld* World = InHLODBuildContext.World;
	UStaticMesh* MergedStaticMesh = nullptr;
	UStaticMesh* ProxyedStaticMesh = nullptr;

	FVector MergedLocation;
	TArray<UPrimitiveComponent*> SourceCompList;
	TArray<UStaticMeshComponent*> TreeCompList;
	for (auto SourceComponent : InSourceComponents) {
		if (auto StaticMeshComponent = Cast<UStaticMeshComponent>(SourceComponent)) {
			if (StaticMeshComponent->GetStaticMesh().GetName().Contains("Tree")) {
				TreeCompList.AddUnique(StaticMeshComponent);
			}
			else {
				SourceCompList.AddUnique(StaticMeshComponent);
			}
		}
	}
	TArray<UActorComponent*> Results;

	if (!TreeCompList.IsEmpty()) {
		FBoxSphereBounds Bounds;
		TMap<UStaticMesh*, TArray<FTransform>> InstancedMap;
		for (auto MeshComp : TreeCompList) {
			UStaticMesh* Mesh = MeshComp->GetStaticMesh();
			auto& InstancedInfo = InstancedMap.FindOrAdd(Mesh);
			Bounds = MeshComp->Bounds + Bounds;
			if (auto ISMC = Cast<UInstancedStaticMeshComponent>(MeshComp)) {
				FTransform Transform;
				for (int i = 0; i < ISMC->GetInstanceCount(); i++) {
					ISMC->GetInstanceTransform(i, Transform, true);
					InstancedInfo.Add(Transform);
				}
			}
			else {
				InstancedInfo.Add(MeshComp->K2_GetComponentToWorld());
			}
		}
		for (const auto& InstancedInfo : InstancedMap) {
			UInstancedStaticMeshComponent* ISMComponent = NewObject<UInstancedStaticMeshComponent>(InHLODBuildContext.AssetsOuter, UInstancedStaticMeshComponent::StaticClass(), *InstancedInfo.Key->GetName(), RF_Transactional);
			ISMComponent->Mobility = EComponentMobility::Static;
			ISMComponent->SetStaticMesh(InstancedInfo.Key);
			ISMComponent->SetForceDisableNanite(true);
			ISMComponent->SetForcedLodModel(8);
			ISMComponent->SetWorldLocation(Bounds.Origin);
			for (auto InstanceTransform : InstancedInfo.Value) {
				ISMComponent->AddInstance(InstanceTransform, true);
			}
			Results.Add(ISMComponent);
		}
	}

	TArray<UObject*> AssetsToSync;
	MeshMergeUtilities.MergeComponentsToStaticMesh(SourceCompList, nullptr, MergeSettings, nullptr, GetTransientPackage(), "MergedStaticMesh", AssetsToSync, MergedLocation, TNumericLimits<float>::Max(), true);
	for (auto Asset : AssetsToSync) {
		if (auto StaticMesh = Cast<UStaticMesh>(Asset)) {
			MergedStaticMesh = StaticMesh;
		}
		else if (auto Texture = Cast<UTexture2D>(Asset)) {
			Texture->Filter = TF_Nearest;
			Texture->MipGenSettings =  TMGS_NoMipmaps;
			Texture->UpdateResource();
			Texture->PostEditChange();
			Texture->MarkPackageDirty();

		}
	}
	if (MergedStaticMesh) {
		AStaticMeshActor* StatcMeshActor = World->SpawnActor<AStaticMeshActor>(MergedLocation, FRotator());
		StatcMeshActor->SetFlags(RF_Transient);
		UStaticMeshComponent* StaticMeshComp = StatcMeshActor->GetStaticMeshComponent();
		StaticMeshComp->SetFlags(RF_Transient);
		StaticMeshComp->SetStaticMesh(MergedStaticMesh);
		Results.Append(Super::Build(InHLODBuildContext, { StaticMeshComp }));
		return Results;
	}
	return {};
}

