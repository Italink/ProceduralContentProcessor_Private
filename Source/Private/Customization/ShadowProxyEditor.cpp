#include "ShadowProxyEditor.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include <Kismet/GameplayStatics.h>
#include "Components/SceneCaptureComponent2D.h"
#include "LevelEditorSubsystem.h"
#include "LevelEditor.h"
#include "SLevelViewport.h"
#include "Selection.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialInstanceConstant.h"
#include "ProceduralContentProcessorLibrary.h"
#include "Engine/StaticMeshActor.h"

void UShadowProxyEditor::Activate()
{
	UWorld* World = GetWorld();
	if (!CaptureRT) {
		CaptureRT = NewObject<UTextureRenderTarget2D>();
		CaptureRT->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
		CaptureRT->InitAutoFormat(2048, 2048);
		CaptureRT->UpdateResourceImmediate(true);
	}
	if (!CaptureActor) {
		TArray<AActor*> CaptureActors;
		UGameplayStatics::GetAllActorsOfClass(World, ASceneCapture2D::StaticClass(), CaptureActors);
		for (auto Actor : CaptureActors) {
			if (Actor->Tags.Contains("BillboardCapture")) {
				CaptureActor = Cast<ASceneCapture2D>(Actor);
				break;
			}
		}
		if (!CaptureActor) {
			CaptureActor = World->SpawnActor<ASceneCapture2D>();
			CaptureActor->SetFlags(RF_Transient);
		}
		USceneCaptureComponent2D* SceneCaptureComp = CaptureActor->GetCaptureComponent2D();
		SceneCaptureComp->TextureTarget = CaptureRT;
		SceneCaptureComp->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;

		ULevelEditorSubsystem* LevelEditorSubsystem = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>();
		if (LevelEditorSubsystem){
			LevelEditorSubsystem->PilotLevelActor(CaptureActor);
			FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
			TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule.GetFirstLevelEditor();
			if (LevelEditor){
				TSharedPtr<SLevelViewport> ActiveLevelViewport = LevelEditor->GetActiveViewportInterface();
				if (ActiveLevelViewport && !ActiveLevelViewport->IsLockedCameraViewEnabled()){
					ActiveLevelViewport->ToggleActorPilotCameraView();
				}
			}
		}
	}
	if (!CaptureResult) {
		UStaticMesh* PlaneMesh  = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
		CaptureResult = DuplicateObject<UStaticMesh>(PlaneMesh, GetTransientPackage());
		if (auto BillboardMaterialObject = Cast<UMaterialInterface>(BillboardMaterial.TryLoad())) {
			BillboardMaterialMID = UKismetMaterialLibrary::CreateDynamicMaterialInstance(nullptr, BillboardMaterialObject);
			CaptureResult->SetMaterial(0, BillboardMaterialMID);
		}
	}
	FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	OnActorSelectionChangedHandle = LevelEditor.OnActorSelectionChanged().AddUObject(this, &UShadowProxyEditor::OnActorSelectionChanged);
	TArray<UObject*> Objects;
	GEditor->GetSelectedActors()->GetSelectedObjects(Objects);
	OnActorSelectionChanged(Objects, true);
	OnRefreshLightActors();
}

void UShadowProxyEditor::Deactivate()
{
	if (!CaptureActor) {
		CaptureActor->K2_DestroyActor();
	}
	if (OnActorSelectionChangedHandle.IsValid()) {
		FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
		LevelEditor.OnActorSelectionChanged().Remove(OnActorSelectionChangedHandle);
		OnActorSelectionChangedHandle.Reset();
	}
}

void UShadowProxyEditor::Tick(const float InDeltaTime)
{
	UWorld* World = GetWorld();
	if (!SourceActors.IsEmpty()) {
		FBoxSphereBounds Bounds;
		Bounds.SphereRadius = 0;
		for (const auto& Actor : SourceActors) {
			for (auto Comp : Actor->GetComponents()) {
				if (auto StaticMeshComp = Cast<UStaticMeshComponent>(Comp)){
					if (Bounds.SphereRadius <= 0) {
						Bounds = StaticMeshComp->Bounds;
					}
					else {
						Bounds = Bounds + StaticMeshComp->Bounds;
					}
				}
			}
		}
		CurrentBounds = Bounds;
		DrawDebugBox(World, CurrentBounds.Origin, CurrentBounds.BoxExtent, FColor::Green, false, -1.0f, 0, 10.0f);

		if (CaptureRT->SizeX != CaptureResolution) {
			UKismetRenderingLibrary::ResizeRenderTarget2D(CaptureRT, CaptureResolution, CaptureResolution);
		}

		FVector Origins[4], Directions[4], WorldPos[4];
		float ResolutionRadius = CaptureResolution * CaptureScale / 2.0f;
		float Distance = (CaptureActor->GetActorLocation() - CurrentBounds.Origin).Length();
		FVector2D ScreenPos = UProceduralContentProcessorLibrary::ProjectWorldToScreen(CurrentBounds.Origin, false);
		UProceduralContentProcessorLibrary::DeprojectScreenToWorld(ScreenPos + FVector2D( -CaptureResolution, -CaptureResolution), Origins[0], Directions[0]);
		UProceduralContentProcessorLibrary::DeprojectScreenToWorld(ScreenPos + FVector2D( -CaptureResolution, +CaptureResolution), Origins[1], Directions[1]);
		UProceduralContentProcessorLibrary::DeprojectScreenToWorld(ScreenPos + FVector2D( +CaptureResolution, -CaptureResolution), Origins[2], Directions[2]);
		UProceduralContentProcessorLibrary::DeprojectScreenToWorld(ScreenPos + FVector2D( +CaptureResolution,  +CaptureResolution), Origins[3], Directions[3]);
		for (int i = 0; i < 4; i++) {
			Directions[i].Normalize();
			WorldPos[i] = Origins[i] + Directions[i] * Distance;
		}
		DrawDebugLine(World, WorldPos[0], WorldPos[1], FColor::Yellow, false, -1.0f, 0, 10.0f);
		DrawDebugLine(World, WorldPos[1], WorldPos[2], FColor::Yellow, false, -1.0f, 0, 10.0f);
		DrawDebugLine(World, WorldPos[2], WorldPos[3], FColor::Yellow, false, -1.0f, 0, 10.0f);
		DrawDebugLine(World, WorldPos[3], WorldPos[0], FColor::Yellow, false, -1.0f, 0, 10.0f);
	}
}

void UShadowProxyEditor::Capture()
{
	CaptureResolution = FMath::RoundUpToPowerOfTwo(CaptureResolution);

	UTexture* Texture = UProceduralContentProcessorLibrary::ConstructTexture2D(CaptureRT, GetTransientPackage(), "BillboardTexture");
	BillboardMaterialMID->SetTextureParameterValue("Texture", Texture);

	UWorld* World = GetWorld();
	AStaticMeshActor* BillboardActor = World->SpawnActor<AStaticMeshActor>();
	BillboardActor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")));
	if (auto BillboardMaterialObject = Cast<UMaterialInterface>(BillboardMaterial.TryLoad())) {
		UMaterialInstanceConstant* NewMaterial = NewObject<UMaterialInstanceConstant>(BillboardActor->GetPackage());
		NewMaterial->SetParentEditorOnly(BillboardMaterialObject);
		NewMaterial->SetTextureParameterValueEditorOnly(FMaterialParameterInfo("Texture"), DuplicateObject<UTexture>(Texture, BillboardActor->GetPackage()));
		NewMaterial->PreEditChange(nullptr);
		NewMaterial->PostEditChange();
		BillboardActor->GetStaticMeshComponent()->SetMaterial(0, NewMaterial);
	}
	BillboardActor->SetActorLocation(CurrentBounds.Origin);
	FVector Eye = CaptureActor->GetActorLocation() - BillboardActor->GetActorLocation();
	float Angle = FMath::Atan(FMath::Sqrt(FMath::Square(Eye.X) + FMath::Square(Eye.Y))/ Eye.Z);

	FRotator Rotator = UKismetMathLibrary::RotatorFromAxisAndAngle(FVector(-Eye.Y, Eye.X, 0), Angle / UE_DOUBLE_PI * 180.0f);
	BillboardActor->SetActorRotation(Rotator);

	GEditor->GetSelectedActors()->Modify();
	GEditor->GetSelectedActors()->BeginBatchSelectOperation();
	GEditor->SelectNone(false, true, true);
	GEditor->SelectActor(BillboardActor, true, false, true);
	GEditor->GetSelectedActors()->EndBatchSelectOperation(/*bNotify*/false);
	GEditor->NoteSelectionChange();
}

void UShadowProxyEditor::OnActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh)
{
	SourceActors.Reset();
	for (auto Item : NewSelection) {
		AActor* Actor = Cast<AActor>(Item);
		if (Actor == nullptr || Actor->HasAnyFlags(RF_Transient))
			continue;
		bool bHasMesh = false;
		for (auto Comp : Actor->GetComponents()) {
			if (auto StaticMeshComp = Cast<UStaticMeshComponent>(Comp)) {
				bHasMesh = true;
			}
		}
		if (bHasMesh) {
			SourceActors.AddUnique(Actor);
		}
	}
	USceneCaptureComponent2D* SceneCaptureComp = CaptureActor->GetCaptureComponent2D();
	SceneCaptureComp->ShowOnlyActors = SourceActors;
	SceneCaptureComp->ShowOnlyActors.Append(LightActors);
}

void UShadowProxyEditor::OnRefreshLightActors()
{
	LightActors.Reset();
	UWorld* World = GetWorld();
	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), Actors);
	for (const auto& Actor : Actors) {
		if(Actor->GetComponentByClass(ULightComponent::StaticClass())){
			LightActors.AddUnique(Actor);
		}
	}
}
