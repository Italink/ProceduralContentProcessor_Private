#include "BillboardCaptureTool.h"
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
#include "TextureCompiler.h"
#include "ImageUtils.h"

void UBillboardCaptureTool::Activate()
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
	OnActorSelectionChangedHandle = LevelEditor.OnActorSelectionChanged().AddUObject(this, &UBillboardCaptureTool::OnActorSelectionChanged);
	TArray<UObject*> Objects;
	GEditor->GetSelectedActors()->GetSelectedObjects(Objects);
	OnActorSelectionChanged(Objects, true);
	OnRefreshLightActors();
}

void UBillboardCaptureTool::Deactivate()
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

void UBillboardCaptureTool::Tick(const float InDeltaTime)
{
	UWorld* World = GetWorld();
	if (!SourceActors.IsEmpty()) {
		DrawDebugBox(World, CurrentBounds.Origin, CurrentBounds.BoxExtent, FColor::Green, false, -1.0f, 0, 10.0f);
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
		TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule.GetFirstLevelEditor();
		if (LevelEditor) {
			FIntPoint ViewportSize = LevelEditor->GetActiveViewportSize().IntPoint();
			if (CaptureRT->SizeX != ViewportSize.X || CaptureRT->SizeY != ViewportSize.Y) {
				UKismetRenderingLibrary::ResizeRenderTarget2D(CaptureRT, ViewportSize.X, ViewportSize.Y);
			}
			FVector2D ScreenCenter = UProceduralContentProcessorLibrary::ProjectWorldToScreen(CurrentBounds.Origin, false);
			FVector2D ScreenPositions[] = {
				UProceduralContentProcessorLibrary::ProjectWorldToScreen(CurrentBounds.Origin + FVector(-CurrentBounds.BoxExtent.X, -CurrentBounds.BoxExtent.Y, -CurrentBounds.BoxExtent.Z), false),
				UProceduralContentProcessorLibrary::ProjectWorldToScreen(CurrentBounds.Origin + FVector(-CurrentBounds.BoxExtent.X, -CurrentBounds.BoxExtent.Y, +CurrentBounds.BoxExtent.Z), false),
				UProceduralContentProcessorLibrary::ProjectWorldToScreen(CurrentBounds.Origin + FVector(-CurrentBounds.BoxExtent.X, +CurrentBounds.BoxExtent.Y, -CurrentBounds.BoxExtent.Z), false),
				UProceduralContentProcessorLibrary::ProjectWorldToScreen(CurrentBounds.Origin + FVector(-CurrentBounds.BoxExtent.X, +CurrentBounds.BoxExtent.Y, +CurrentBounds.BoxExtent.Z), false),
				UProceduralContentProcessorLibrary::ProjectWorldToScreen(CurrentBounds.Origin + FVector(+CurrentBounds.BoxExtent.X, -CurrentBounds.BoxExtent.Y, -CurrentBounds.BoxExtent.Z), false),
				UProceduralContentProcessorLibrary::ProjectWorldToScreen(CurrentBounds.Origin + FVector(+CurrentBounds.BoxExtent.X, -CurrentBounds.BoxExtent.Y, +CurrentBounds.BoxExtent.Z), false),
				UProceduralContentProcessorLibrary::ProjectWorldToScreen(CurrentBounds.Origin + FVector(+CurrentBounds.BoxExtent.X, +CurrentBounds.BoxExtent.Y, -CurrentBounds.BoxExtent.Z), false),
				UProceduralContentProcessorLibrary::ProjectWorldToScreen(CurrentBounds.Origin + FVector(+CurrentBounds.BoxExtent.X, +CurrentBounds.BoxExtent.Y, +CurrentBounds.BoxExtent.Z), false),
			};
			CurrentScreenBounds.Min = FVector2D(FLT_MAX, FLT_MAX);
			CurrentScreenBounds.Max = FVector2D(FLT_MIN, FLT_MIN);
			for (auto ScreenPos : ScreenPositions) {
				CurrentScreenBounds.Min.X = FMath::Min(CurrentScreenBounds.Min.X, ScreenPos.X);
				CurrentScreenBounds.Min.Y = FMath::Min(CurrentScreenBounds.Min.Y, ScreenPos.Y);
				CurrentScreenBounds.Max.X = FMath::Max(CurrentScreenBounds.Max.X, ScreenPos.X);
				CurrentScreenBounds.Max.Y = FMath::Max(CurrentScreenBounds.Max.Y, ScreenPos.Y);
			}
		}
		if (BillboardCapturePlane != nullptr) {
			FRotator Rotator(0.0f, -90.0f, 90.0f);
			Rotator.Yaw += UKismetMathLibrary::FindLookAtRotation(CurrentBounds.Origin, CaptureActor->GetActorLocation()).Yaw;
			BillboardCapturePlane->SetActorRotation(Rotator.Quaternion());
			float Asp = (CurrentScreenBounds.Max.X - CurrentScreenBounds.Min.X) / (CurrentScreenBounds.Max.Y - CurrentScreenBounds.Min.Y);
			FVector Scale(0.02f * BillboardCapturePlaneScale.X * CurrentBounds.SphereRadius, 0.02f / Asp * BillboardCapturePlaneScale.Y * CurrentBounds.SphereRadius, 1.0f);
			BillboardCapturePlane->SetActorScale3D(Scale);

			FVector Pos(50,50,0);
			
			DrawDebugBox(World, BillboardCapturePlane->GetTransform().TransformPosition(FVector(50, 50, 0)), FVector(100), FColor::Red);
		}
	}
}

void UBillboardCaptureTool::Capture()
{
	FTransform Transform = BillboardCapturePlane->GetTransform();
	FQuadrangle TargetQuad;
	TargetQuad.BottomLeft() = UProceduralContentProcessorLibrary::ProjectWorldToScreen(Transform.TransformPosition(FVector(-50, 50, 0)), false);
	TargetQuad.BottomRight() = UProceduralContentProcessorLibrary::ProjectWorldToScreen(Transform.TransformPosition(FVector(50, 50, 0)), false);
	TargetQuad.TopRight() = UProceduralContentProcessorLibrary::ProjectWorldToScreen(Transform.TransformPosition(FVector(50, -50, 0)), false);
	TargetQuad.TopLeft() = UProceduralContentProcessorLibrary::ProjectWorldToScreen(Transform.TransformPosition(FVector(-50, -50, 0)), false);
	FIntPoint SourceSize(
		(FMath::Abs(TargetQuad.BottomRight().X - TargetQuad.BottomLeft().X) + FMath::Abs(TargetQuad.TopRight().X - TargetQuad.TopLeft().X)) / 2,
		(FMath::Abs(TargetQuad.BottomLeft().Y - TargetQuad.TopLeft().Y) + FMath::Abs(TargetQuad.BottomRight().Y - TargetQuad.TopRight().Y)) / 2
	);
	FQuadrangle SourceQuad;
	SourceQuad.BottomLeft()		= FVector2D(0, SourceSize.Y);
	SourceQuad.BottomRight()	= FVector2D(SourceSize.X, SourceSize.Y);
	SourceQuad.TopRight()		= FVector2D(SourceSize.X, 0);
	SourceQuad.TopLeft()		= FVector2D(0, 0);

	FMatrix TransformMatrix = FQuadrangle::CalcTransformMaterix(SourceQuad, TargetQuad);
	ETextureRenderTargetFormat RenderTargetFormat = CaptureRT->RenderTargetFormat;
	FTextureRenderTargetResource* RenderTargetResource = CaptureRT->GameThread_GetRenderTargetResource();
	TArray<FColor> FullColors;
	FReadSurfaceDataFlags ReadPixelFlags(RCM_MinMax);
	RenderTargetResource->ReadPixels(FullColors, ReadPixelFlags);

	TArray<FColor> ResolvedColors;
	ResolvedColors.AddUninitialized(SourceSize.X * SourceSize.Y);
	for (int i = 0; i < SourceSize.X; i++) {
		for(int j = 0; j< SourceSize.Y; j++){
			FVector2D SamplePosition = FQuadrangle::TransformPoint(TransformMatrix, FVector2D(i, j));
			FIntPoint SampleIntPosition(FMath::RoundToInt(SamplePosition.X), FMath::RoundToInt(SamplePosition.Y));
			FColor SampleColor = FColor::Black;
			if (SampleIntPosition.X >= 0 && SampleIntPosition.X < CaptureRT->SizeX
				&& SampleIntPosition.Y >= 0 && SampleIntPosition.Y < CaptureRT->SizeY) {
				int SourceIndex = SampleIntPosition.Y * CaptureRT->SizeX + SampleIntPosition.X;
				SampleColor = FullColors[SourceIndex];
			}
			ResolvedColors[j * SourceSize.X + i] = SampleColor;
		}
	}
	FCreateTexture2DParameters Params;
	Params.CompressionSettings = TC_Default;
	Params.bUseAlpha = true;
	BillboardTexture = FImageUtils::CreateTexture2D(SourceSize.X , SourceSize.Y, ResolvedColors, GetTransientPackage(), "BillboardTexture", EObjectFlags::RF_NoFlags, Params);
	//BillboardTexture = UProceduralContentProcessorLibrary::ConstructTexture2DByRegion(CaptureRT, CurrentScreenBounds, GetTransientPackage(), "BillboardTexture");

	BillboardMaterialMID->SetTextureParameterValue("Texture", BillboardTexture);

	UWorld* World = GetWorld();
	if (auto BillboardMaterialObject = Cast<UMaterialInterface>(BillboardMaterial.TryLoad())) {
		UMaterialInstanceConstant* NewMaterial = NewObject<UMaterialInstanceConstant>(BillboardCapturePlane->GetPackage());
		NewMaterial->SetParentEditorOnly(BillboardMaterialObject);
		NewMaterial->SetTextureParameterValueEditorOnly(FMaterialParameterInfo("Texture"), DuplicateObject<UTexture2D>(BillboardTexture, BillboardCapturePlane->GetPackage()));
		NewMaterial->PreEditChange(nullptr);
		NewMaterial->PostEditChange();
		BillboardCapturePlane->GetStaticMeshComponent()->SetMaterial(0, NewMaterial);
	}
}

void UBillboardCaptureTool::OnActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh)
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
	FBoxSphereBounds Bounds;
	Bounds.SphereRadius = 0;
	for (const auto& Actor : SourceActors) {
		for (auto Comp : Actor->GetComponents()) {
			if (auto StaticMeshComp = Cast<UStaticMeshComponent>(Comp)) {
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
	USceneCaptureComponent2D* SceneCaptureComp = CaptureActor->GetCaptureComponent2D();
	SceneCaptureComp->ShowOnlyActors = SourceActors;
	SceneCaptureComp->ShowOnlyActors.Append(LightActors);
	OnRefreshCapturePlane();
}

void UBillboardCaptureTool::OnRefreshLightActors()
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

void UBillboardCaptureTool::OnRefreshCapturePlane()
{
	UWorld* World = GetWorld();
	if (BillboardCapturePlane == nullptr) {
		BillboardCapturePlane = World->SpawnActor<AStaticMeshActor>();
		BillboardCapturePlane->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")));
	}
	BillboardCapturePlane->SetActorLocation(CurrentBounds.Origin);

	//BillboardCapturePlane->GetActorBounds(false, Center, Extents);
	//FVector2D ScreenCenter = UProceduralContentProcessorLibrary::ProjectWorldToScreen(Center, false);
	//FVector2D ScreenPositions[] = {
	//	UProceduralContentProcessorLibrary::ProjectWorldToScreen(Center + FVector(-Extents.X, -Extents.Y, -Extents.Z), false),
	//	UProceduralContentProcessorLibrary::ProjectWorldToScreen(Center + FVector(-Extents.X, -Extents.Y, +Extents.Z), false),
	//	UProceduralContentProcessorLibrary::ProjectWorldToScreen(Center + FVector(-Extents.X, +Extents.Y, -Extents.Z), false),
	//	UProceduralContentProcessorLibrary::ProjectWorldToScreen(Center + FVector(-Extents.X, +Extents.Y, +Extents.Z), false),
	//	UProceduralContentProcessorLibrary::ProjectWorldToScreen(Center + FVector(+Extents.X, -Extents.Y, -Extents.Z), false),
	//	UProceduralContentProcessorLibrary::ProjectWorldToScreen(Center + FVector(+Extents.X, -Extents.Y, +Extents.Z), false),
	//	UProceduralContentProcessorLibrary::ProjectWorldToScreen(Center + FVector(+Extents.X, +Extents.Y, -Extents.Z), false),
	//	UProceduralContentProcessorLibrary::ProjectWorldToScreen(Center + FVector(+Extents.X, +Extents.Y, +Extents.Z), false),
	//};
	//FVector2D MinBoxDistance(FLT_MAX, FLT_MAX);
	//for (auto ScreenPos : ScreenPositions) {
	//	MinBoxDistance.X = FMath::Min(FMath::Abs(ScreenPos.X - ScreenCenter.X), MinBoxDistance.X);
	//	MinBoxDistance.Y = FMath::Min(FMath::Abs(ScreenPos.Y - ScreenCenter.Y), MinBoxDistance.Y);
	//}
	//Scale = FVector(CurrentMaxBoxDistance.X / MinBoxDistance.X, CurrentMaxBoxDistance.Y / MinBoxDistance.Y, 1.0f);
	//Scale *= BillboardCapturePlane->GetActorScale3D();
	//if (Scale.Length() <= 0.01f) {
	//	Scale = FVector(0.01f);
	//}
	//BillboardCapturePlane->SetActorScale3D(Scale);
	
}

FMatrix FQuadrangle::CalcTransformMaterix(FQuadrangle InSrc, FQuadrangle InDst)
{
	FMatrix Mat;

	double X0 = InSrc.Vertices[0].X, X1 = InSrc.Vertices[1].X, X2 = InSrc.Vertices[3].X, X3 = InSrc.Vertices[2].X;
	double Y0 = InSrc.Vertices[0].Y, Y1 = InSrc.Vertices[1].Y, Y2 = InSrc.Vertices[3].Y, Y3 = InSrc.Vertices[2].Y;
	double U0 = InDst.Vertices[0].X, U1 = InDst.Vertices[1].X, U2 = InDst.Vertices[3].X, U3 = InDst.Vertices[2].X;
	double V0 = InDst.Vertices[0].Y, V1 = InDst.Vertices[1].Y, V2 = InDst.Vertices[3].Y, V3 = InDst.Vertices[2].Y;

	double Gauss[8][9] = {
	   { X0, Y0, 1, 0, 0, 0, -X0 * U0, -Y0 * U0, U0 },
	   { X1, Y1, 1, 0, 0, 0, -X1 * U1, -Y1 * U1, U1 },
	   { X2, Y2, 1, 0, 0, 0, -X2 * U2, -Y2 * U2, U2 },
	   { X3, Y3, 1, 0, 0, 0, -X3 * U3, -Y3 * U3, U3 },
	   { 0, 0, 0, X0, Y0, 1, -X0 * V0, -Y0 * V0, V0 },
	   { 0, 0, 0, X1, Y1, 1, -X1 * V1, -Y1 * V1, V1 },
	   { 0, 0, 0, X2, Y2, 1, -X2 * V2, -Y2 * V2, V2 },
	   { 0, 0, 0, X3, Y3, 1, -X3 * V3, -Y3 * V3, V3 },
	};

	for (int row = 0, col = 0; col < 8 && row < 8; col++, row++) {      
		int maX_r = row;
		for (int i = row + 1; i < 8; i++) {
			if ((1e-12) < FMath::Abs(Gauss[i][col]) - FMath::Abs(Gauss[maX_r][col])) {
				maX_r = i;
			}
		}
		if (maX_r != row)
			for (int j = 0; j < 9; j++)
				Swap(Gauss[row][j], Gauss[maX_r][j]);
		for (int i = row + 1; i < 8; i++) {
			if (FMath::Abs(Gauss[i][col]) < (1e-12))
				continue;
			double tmp = -Gauss[i][col] / Gauss[row][col];
			for (int j = col; j < 9; j++) {
				Gauss[i][j] += tmp * Gauss[row][j];
			}
		}
	}

	for (int i = 7; i >= 0; i--) {										
		double tmp = 0;
		for (int j = i + 1; j < 8; j++) {
			tmp += Gauss[i][j] * Mat.M[j / 3][j % 3];
		}
		Mat.M[i / 3][i % 3] = (Gauss[i][8] - tmp) / Gauss[i][i];
	}

	Mat.M[2][2] = 1;
	Mat.M[3][3] = 1;

	return Mat.GetTransposed();
}

FVector2D FQuadrangle::TransformPoint(const FMatrix& InMat, const FVector2D& InPoint)
{
	FVector2D Result;
	double Z = InPoint.X * InMat.M[0][2] + InPoint.Y * InMat.M[1][2] + InMat.M[2][2];
	Result.X = (InPoint.X * InMat.M[0][0] + InPoint.Y * InMat.M[1][0] + InMat.M[2][0]) / Z;
	Result.Y = (InPoint.X * InMat.M[0][1] + InPoint.Y * InMat.M[1][1] + InMat.M[2][1]) / Z;
	return Result;
}
