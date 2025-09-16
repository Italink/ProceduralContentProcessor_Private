#include "FoliagePartitionTool.h"
#include "Engine/Engine.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/Selection.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Layers/LayersSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMeshActor.h"
#include "InstancedFoliageActor.h"


void UFoliagePartitionTool::Activate()
{
	StaticMeshes.Reset();
	for (auto MeshPath : StaticMeshesForConfig) {
		StaticMeshes.AddUnique(Cast<UStaticMesh>(MeshPath.TryLoad()));
	}
}

void UFoliagePartitionTool::Deactivate()
{

}

void UFoliagePartitionTool::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	const FName PropertyName = (PropertyChangedEvent.Property != NULL) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UFoliagePartitionTool, StaticMeshes)) {
		StaticMeshesForConfig.Reset();
		for (auto Mesh : StaticMeshes) {
			StaticMeshesForConfig.AddUnique(Mesh);
		}
	}
}


void UFoliagePartitionTool::ToggleFoliagePartition()
{
	auto IsVaild = [this]()
	{
		if (GEditor->GetSelectedActorCount() != 1)
			return false;
		AActor* SelectActor = GEditor->GetSelectedActors()->GetTop<AActor>();
		if (SelectActor == nullptr)
			return false;
		if (auto MeshActor = Cast<AStaticMeshActor>(SelectActor)) {
			UStaticMesh* Mesh = MeshActor->GetStaticMeshComponent()->GetStaticMesh();
			if (Mesh && StaticMeshes.Contains(Mesh)) {
				return true;
			}
		}
		else if (SelectActor->GetActorLabel().StartsWith("FoliagePartition_")) {
			return true;
		}
		return false;
	}();
	if (!IsVaild) {
		FNotificationInfo Info(FText::FromString(TEXT("The currently selected actor is not a foliage mesh")));
		Info.FadeInDuration = 2.0f;
		Info.ExpireDuration = 2.0f;
		Info.FadeOutDuration = 2.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
		return;
	}
	AActor* SelectActor = GEditor->GetSelectedActors()->GetTop<AActor>();
	if (SelectActor == nullptr)
		return;
	UWorld* World = SelectActor->GetWorld();
	if (Cast<AStaticMeshActor>(SelectActor)) {
		ULayersSubsystem* LayersSubsystem = GEditor->GetEditorSubsystem<ULayersSubsystem>();
		TMap<FIntPoint, TArray<AActor*>> PendingMergeActors;
		TMap<FIntPoint, AActor*> FoliagePartitionActors;
		TArray<AActor*> AllActors;
		UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);
		for (auto Actor : AllActors) {
			if (AStaticMeshActor* MeshActor = Cast<AStaticMeshActor>(Actor)) {
				UStaticMesh* Mesh = MeshActor->GetStaticMeshComponent()->GetStaticMesh();
				if (Mesh && StaticMeshes.Contains(Mesh)) {
					FVector Position = Actor->K2_GetActorLocation();
					FIntPoint CellCoord(FMath::Floor((Position.X + Origin.X) / CellSize), FMath::Floor((Position.Y + Origin.Y) / CellSize));
					PendingMergeActors.FindOrAdd(CellCoord).Add(Actor);
				}
			}
			else if (Actor->GetActorLabel().StartsWith("FoliagePartition_")) {
				TArray<FString> Seg;
				Actor->GetActorLabel().ParseIntoArray(Seg, TEXT("_"));
				FIntPoint CellCoord(FCString::Atoi(*Seg[1]), FCString::Atoi(*Seg[2]));
				FoliagePartitionActors.Add(CellCoord, Actor);
			}
		}
		for (auto SourceActors : PendingMergeActors) {
			AActor* FoliagePartitionActor = nullptr;
			FBoxSphereBounds Bounds;
			TMap<UStaticMesh*, TArray<FTransform>> InstancedMap;
			bool bNeedInitNewActor = false;
			for (auto Actor : SourceActors.Value) {
				TArray<UStaticMeshComponent*> MeshComps;
				Actor->GetComponents(MeshComps, true);
				for (auto MeshComp : MeshComps) {
					UStaticMesh* Mesh = MeshComp->GetStaticMesh();
					auto& InstancedInfo = InstancedMap.FindOrAdd(Mesh);
					Bounds = MeshComp->Bounds + Bounds;
					FTransform Transform;
					if (auto ISMC = Cast<UInstancedStaticMeshComponent>(MeshComp)) {
						for (int i = 0; i < ISMC->GetInstanceCount(); i++) {
							ISMC->GetInstanceTransform(i, Transform, true);
							Transform.NormalizeRotation();
							InstancedInfo.Add(Transform);
						}
					}
					else {
						Transform = MeshComp->K2_GetComponentToWorld();
						Transform.NormalizeRotation();
						InstancedInfo.Add(Transform);
					}
				}
			}
			if (FoliagePartitionActors.Contains(SourceActors.Key)) {
				FoliagePartitionActor = FoliagePartitionActors[SourceActors.Key];
			}
			else {
				FActorSpawnParameters SpawnInfo;
				SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				FTransform Transform;
				Transform.SetLocation(Bounds.Origin);
				FoliagePartitionActor = World->SpawnActor<AActor>(AActor::StaticClass(), Transform, SpawnInfo);
				USceneComponent* RootComponent = NewObject<USceneComponent>(FoliagePartitionActor, USceneComponent::GetDefaultSceneRootVariableName(), RF_Transactional);
				RootComponent->Mobility = EComponentMobility::Static;
				RootComponent->bVisualizeComponent = true;
				FoliagePartitionActor->SetRootComponent(RootComponent);
				FoliagePartitionActor->AddInstanceComponent(RootComponent);
				RootComponent->OnComponentCreated();
				RootComponent->RegisterComponent();
				FoliagePartitionActor->SetActorLabel(FString::Printf(TEXT("FoliagePartition_%d_%d"), SourceActors.Key.X, SourceActors.Key.Y));
				bNeedInitNewActor = true;
			}
			for (const auto& InstancedInfo : InstancedMap) {
				UHierarchicalInstancedStaticMeshComponent* HISMComponent = nullptr;
				TArray<UHierarchicalInstancedStaticMeshComponent*> HISMComps;
				FoliagePartitionActor->GetComponents(HISMComps, true);
				for (auto HISMC : HISMComps) {
					if (HISMC->GetStaticMesh() == InstancedInfo.Key) {
						HISMComponent = HISMC;
						break;
					}
				}
				if (HISMComponent == nullptr) {
					HISMComponent = NewObject<UHierarchicalInstancedStaticMeshComponent>(FoliagePartitionActor, UHierarchicalInstancedStaticMeshComponent::StaticClass(), *InstancedInfo.Key->GetName(), RF_Transactional);
					HISMComponent->Mobility = EComponentMobility::Static;
					FoliagePartitionActor->AddInstanceComponent(HISMComponent);
					HISMComponent->AttachToComponent(FoliagePartitionActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
					HISMComponent->OnComponentCreated();
					HISMComponent->RegisterComponent();
					HISMComponent->SetStaticMesh(InstancedInfo.Key);
				}
				for (auto InstanceTransform : InstancedInfo.Value) {
					HISMComponent->AddInstance(InstanceTransform, true);
				}
			}
			FoliagePartitionActor->Modify();
			if (bNeedInitNewActor) {
				LayersSubsystem->InitializeNewActorLayers(FoliagePartitionActor);
			}
			GUnrealEd->GetSelectedActors()->Modify();
			GUnrealEd->SelectActor(FoliagePartitionActor, true, false);
			for (auto SourceActor : SourceActors.Value) {
				LayersSubsystem->DisassociateActorFromLayers(SourceActor);
				World->EditorDestroyActor(SourceActor, true);
			}
		}
		FNotificationInfo Info(FText::FromString(TEXT("Foliage Partition Merge Finished")));
		Info.FadeInDuration = 2.0f;
		Info.ExpireDuration = 2.0f;
		Info.FadeOutDuration = 2.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
	else if (SelectActor->GetActorLabel().StartsWith("FoliagePartition_")) {
		ULayersSubsystem* LayersSubsystem = GEditor->GetEditorSubsystem<ULayersSubsystem>();
		TArray<UInstancedStaticMeshComponent*> InstanceComps;
		SelectActor->GetComponents(InstanceComps, true);
		if (!InstanceComps.IsEmpty()) {
			for (auto& ISMC : InstanceComps) {
				for (int i = 0; i < ISMC->GetInstanceCount(); i++) {
					FActorSpawnParameters SpawnInfo;
					SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					FTransform Transform;
					ISMC->GetInstanceTransform(i, Transform, true);
					auto NewActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Transform, SpawnInfo);
					NewActor->GetStaticMeshComponent()->SetStaticMesh(ISMC->GetStaticMesh());
					NewActor->SetActorLabel(SelectActor->GetActorLabel());
					auto Materials = ISMC->GetMaterials();
					for (int j = 0; j < Materials.Num(); j++)
						NewActor->GetStaticMeshComponent()->SetMaterial(j, Materials[j]);
					NewActor->Modify();
					NewActor->SetActorLabel(MakeUniqueObjectName(World, AStaticMeshActor::StaticClass(), *ISMC->GetStaticMesh()->GetName()).ToString());
					LayersSubsystem->InitializeNewActorLayers(NewActor);
					const bool bCurrentActorSelected = GUnrealEd->GetSelectedActors()->IsSelected(SelectActor);
					if (bCurrentActorSelected)
					{
						// The source actor was selected, so deselect the old actor and select the new one.
						GUnrealEd->GetSelectedActors()->Modify();
						GUnrealEd->SelectActor(NewActor, bCurrentActorSelected, false);
						GUnrealEd->SelectActor(SelectActor, false, false);
					}
					{
						LayersSubsystem->DisassociateActorFromLayers(NewActor);
						NewActor->Layers.Empty();
						LayersSubsystem->AddActorToLayers(NewActor, SelectActor->Layers);
					}
				}
				ISMC->Modify();
				ISMC->PerInstanceSMData.Reset();
				ISMC->ClearInstances();
				GUnrealEd->NoteSelectionChange();
			}
		}
		FNotificationInfo Info(FText::FromString(TEXT("Foliage Partition Break Finished")));
		Info.FadeInDuration = 2.0f;
		Info.ExpireDuration = 2.0f;
		Info.FadeOutDuration = 2.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
}

void UFoliagePartitionTool::BreakAllHISM()
{
	UWorld* World = GetWorld();
	TArray<AActor*> AllActors;
	UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);
	for (auto Actor : AllActors) {
		if (Actor->GetActorLabel().StartsWith("FoliagePartition_")) {
			ULayersSubsystem* LayersSubsystem = GEditor->GetEditorSubsystem<ULayersSubsystem>();
			TArray<UInstancedStaticMeshComponent*> InstanceComps;
			Actor->GetComponents(InstanceComps, true);
			if (!InstanceComps.IsEmpty()) {
				for (auto& ISMC : InstanceComps) {
					for (int i = 0; i < ISMC->GetInstanceCount(); i++) {
						FActorSpawnParameters SpawnInfo;
						SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
						FTransform Transform;
						ISMC->GetInstanceTransform(i, Transform, true);
						auto NewActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Transform, SpawnInfo);
						NewActor->GetStaticMeshComponent()->SetStaticMesh(ISMC->GetStaticMesh());
						NewActor->SetActorLabel(Actor->GetActorLabel());
						auto Materials = ISMC->GetMaterials();
						for (int j = 0; j < Materials.Num(); j++)
							NewActor->GetStaticMeshComponent()->SetMaterial(j, Materials[j]);
						NewActor->Modify();
						NewActor->SetActorLabel(MakeUniqueObjectName(World, AStaticMeshActor::StaticClass(), *ISMC->GetStaticMesh()->GetName()).ToString());
						LayersSubsystem->InitializeNewActorLayers(NewActor);
					}
					ISMC->Modify();
					ISMC->PerInstanceSMData.Reset();
					ISMC->ClearInstances();
					GUnrealEd->NoteSelectionChange();
				}
				World->EditorDestroyActor(Actor, true);
			}
		}
	}
}

void UFoliagePartitionTool::Fixup()
{
	// 定义判断两个变换是否视为"重复"的阈值
	const float LOCATION_THRESHOLD = 1.0f; // 位置阈值（厘米）
	const float ROTATION_THRESHOLD = 0.5f; // 旋转阈值（度）

	UWorld* World = GetWorld();
	TArray<AActor*> AllActors;
	UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);

	for (auto Actor : AllActors) {
		if (Actor->GetActorLabel().StartsWith("FoliagePartition_")) {
			TArray<UInstancedStaticMeshComponent*> InstanceComps;
			Actor->GetComponents(InstanceComps, true);

			if (!InstanceComps.IsEmpty()) {
				for (auto& ISMC : InstanceComps) {
					TArray<FTransform> UniqueTransforms;
					TArray<int32> InstancesToRemove;

					// 收集所有实例并标记重复项
					for (int32 i = 0; i < ISMC->GetInstanceCount(); i++) {
						FTransform CurrentTransform;
						ISMC->GetInstanceTransform(i, CurrentTransform, true);

						bool bIsDuplicate = false;

						// 与已收集的唯一变换比较
						for (const auto& UniqueTransform : UniqueTransforms) {
							// 检查位置是否足够接近
							float LocationDistance = FVector::Distance(
								CurrentTransform.GetLocation(),
								UniqueTransform.GetLocation()
							);

							// 检查旋转是否足够接近
							FRotator CurrentRot = CurrentTransform.Rotator();
							FRotator UniqueRot = UniqueTransform.Rotator();
							float RotationDiff = FMath::Max3(
								FMath::Abs(CurrentRot.Pitch - UniqueRot.Pitch),
								FMath::Abs(CurrentRot.Yaw - UniqueRot.Yaw),
								FMath::Abs(CurrentRot.Roll - UniqueRot.Roll)
							);

							// 如果位置和旋转都在阈值内，则视为重复
							if (LocationDistance <= LOCATION_THRESHOLD &&
								RotationDiff <= ROTATION_THRESHOLD) {
								bIsDuplicate = true;
								break;
							}
						}

						if (bIsDuplicate) {
							InstancesToRemove.Add(i);
						}
						else {
							UniqueTransforms.Add(CurrentTransform);
						}
					}

					// 移除重复实例（注意要从后往前删，避免索引错乱）
					if (!InstancesToRemove.IsEmpty()) {
						// 排序并反转，确保从高索引开始删除
						InstancesToRemove.Sort();

						const int32 Count = InstancesToRemove.Num();
						for (int32 i = 0; i < Count / 2; i++) {
							Swap(InstancesToRemove[i], InstancesToRemove[Count - 1 - i]);
						}
						for (int32 Index : InstancesToRemove) {
							ISMC->RemoveInstance(Index);
						}

						// 通知组件更新
						ISMC->MarkRenderStateDirty();
						UE_LOG(LogTemp, Log, TEXT("Removed %d duplicate instances from ISMC"), InstancesToRemove.Num());
					}
				}
			}
		}
	}
}