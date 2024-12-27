#include "GaussDatasetCaptureTool.h"
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
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"

void UGaussDatasetCaptureTool::Activate()
{
	UWorld* World = GetWorld();
	if (!CaptureRT) {
		CaptureRT = NewObject<UTextureRenderTarget2D>();
		CaptureRT->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
		CaptureRT->InitAutoFormat(1024, 1024);
		CaptureRT->UpdateResourceImmediate(true);
	}
	if (!CaptureActor) {
		TArray<AActor*> CaptureActors;
		UGameplayStatics::GetAllActorsOfClass(World, ASceneCapture2D::StaticClass(), CaptureActors);
		for (auto Actor : CaptureActors) {
			if (Actor->Tags.Contains("GaussDatasetCapture")) {
				CaptureActor = Cast<ASceneCapture2D>(Actor);
				break;
			}
		}
		if (!CaptureActor) {
			CaptureActor = World->SpawnActor<ASceneCapture2D>();
			CaptureActor->SetFlags(RF_Transient);
		}
		USceneCaptureComponent2D* SceneCaptureComp = CaptureActor->GetCaptureComponent2D();
		//SceneCaptureComp->ProjectionType = ECameraProjectionMode::Orthographic;
		SceneCaptureComp->bCaptureEveryFrame = false;
		SceneCaptureComp->bCaptureOnMovement = false;
		SceneCaptureComp->TextureTarget = CaptureRT;
		SceneCaptureComp->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("Lighting"), false });
		SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("Atmosphere"), false });
		//SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("Bloom"), false });
		SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("Fog"), false });
		//SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("DynamicShadows"), false });
		//SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("BSP"), false });
		//SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("Cloud"), false });
		SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("Decals"), false });
		SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("DepthOfField"), false });
		//SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("VolumetricFog"), false });
		//SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("Refraction"), false });
		//SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("ScreenSpaceReflections"), false });
		//SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("AmbientCubemap"), false });
		//SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("ReflectionEnvironment"), false });
		//SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("EyeAdaptation"), false });
		//SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("ColorGrading"), false });
	}
	FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	OnActorSelectionChangedHandle = LevelEditor.OnActorSelectionChanged().AddUObject(this, &UGaussDatasetCaptureTool::OnActorSelectionChanged);
	TArray<UObject*> Objects;
	GEditor->GetSelectedActors()->GetSelectedObjects(Objects);
	OnActorSelectionChanged(Objects, true);
}

void UGaussDatasetCaptureTool::Deactivate()
{
	if (CaptureActor) {
		CaptureActor->K2_DestroyActor();
	}
	if (OnActorSelectionChangedHandle.IsValid()) {
		FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
		LevelEditor.OnActorSelectionChanged().Remove(OnActorSelectionChangedHandle);
		OnActorSelectionChangedHandle.Reset();
	}
}

void UGaussDatasetCaptureTool::Tick(const float InDeltaTime)
{
	UWorld* World = GetWorld();
}

FVector UVtoPyramid(FVector2D UV) {
	FVector Position = FVector(
		0.0f + (UV.X - UV.Y),
		-1.0f + (UV.X + UV.Y),
		0.0f
	);

	FVector2D Absolute = FVector2D(Position).GetAbs();
	Position.Z = 1.0f - Absolute.X - Absolute.Y;
	return Position;
}

FVector UVtoOctahedron(FVector2D uv) {
	FVector Position = FVector(2.0f * (uv - 0.5f), 0);

	FVector2D Absolute = FVector2D(Position).GetAbs();
	Position.Z = 1.0f - Absolute.X - Absolute.Y;

	// "Tuck in" the corners by reflecting the xy position along the line y = 1 - x
	// (in quadrant 1), and its mirrored image in the other quadrants.
	if (Position.Z < 0) {
		FVector2D Temp = FMath::Sign(FVector2D(Position))* FVector2D(1.0f - Absolute.X, 1.0f - Absolute.Y);
		Position.X = Temp.X;
		Position.Y = Temp.Y;
	}

	return Position;
}

void UGaussDatasetCaptureTool::Capture()
{
	float Radius = FMath::Max<FVector::FReal>(CurrentBounds.BoxExtent.Size(), 10.f);
	USceneCaptureComponent2D* SceneCaptureComp = CaptureActor->GetCaptureComponent2D();
	const float HalfFOVRadians = FMath::DegreesToRadians(SceneCaptureComp->FOVAngle / 2.0f);
	const float DistanceFromSphere = Radius / FMath::Tan(HalfFOVRadians) * 2;
	for (int i = 0; i < FrameXY; i++) {
		for (int j = 0; j < FrameXY; j++) {
			int FrameIndex = j * FrameXY + i;
			FVector Direction = UVtoPyramid(FVector2D(i / (double)FrameXY, j / (double)FrameXY));
			FVector Position = CurrentBounds.Origin + Direction * DistanceFromSphere;
			FRotator Rotator = UKismetMathLibrary::FindLookAtRotation(Position, CurrentBounds.Origin);
			CaptureActor->SetActorLocationAndRotation(Position, Rotator);
			SceneCaptureComp->CaptureScene();
			TArray<FColor> FullColors;
			ETextureRenderTargetFormat RenderTargetFormat = CaptureRT->RenderTargetFormat;
			FTextureRenderTargetResource* RenderTargetResource = CaptureRT->GameThread_GetRenderTargetResource();
			FReadSurfaceDataFlags ReadPixelFlags(RCM_MinMax);
			FIntRect IntRegion(0, 0, CaptureRT->SizeX, CaptureRT->SizeY);
			RenderTargetResource->ReadPixels(FullColors, ReadPixelFlags, IntRegion);
			for (auto& Color : FullColors) {
				Color.A = 255 - Color.A;
			}
			FImageView ImageView(FullColors.GetData(), CaptureRT->SizeX, CaptureRT->SizeY, EGammaSpace::Linear);
			FImageUtils::SaveImageByExtension(*FString::Printf(TEXT("F:/gaussian-splatting/dataset_ue/Images/%4d.png"),FrameIndex), ImageView);
		}
	}
}

void UGaussDatasetCaptureTool::CreateAsset()
{

	//if (!FrameTexture) {
	//	return;
	//}
	//FSaveAssetDialogConfig SaveAssetDialogConfig;
	//SaveAssetDialogConfig.DefaultPath = LastSavePath;
	//SaveAssetDialogConfig.DefaultAssetName = CurrentName;
	//SaveAssetDialogConfig.AssetClassNames.Add(UTexture2D::StaticClass()->GetClassPathName());
	//SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::AllowButWarn;
	//SaveAssetDialogConfig.DialogTitleOverride = FText::FromString("Save As");

	//const FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	//const FString SaveObjectPath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);
	//if (SaveObjectPath.IsEmpty())
	//{
	//	FNotificationInfo NotifyInfo(FText::FromString("Path is empty"));
	//	NotifyInfo.ExpireDuration = 5.0f;
	//	FSlateNotificationManager::Get().AddNotification(NotifyInfo);
	//	return;
	//}
	//const FString PackagePath = FPackageName::ObjectPathToPackageName(SaveObjectPath);
	//const FString AssetName = FPaths::GetBaseFilename(PackagePath, true);
	//LastSavePath = PackagePath;
	//if (!AssetName.IsEmpty()) {
	//	UPackage* NewPackage = CreatePackage(*PackagePath);
	//	UTexture2D* NewAsset = DuplicateObject<UTexture2D>(FrameTexture, NewPackage, *AssetName);
	//	NewAsset->SetFlags(RF_Public | RF_Standalone);
	//	FAssetRegistryModule::AssetCreated(NewAsset);
	//	FPackagePath NewPackagePath = FPackagePath::FromPackageNameChecked(NewPackage->GetName());
	//	FString PackageLocalPath = NewPackagePath.GetLocalFullPath();
	//	UPackage::SavePackage(NewPackage, NewAsset, RF_Public | RF_Standalone, *PackageLocalPath, GError, nullptr, false, true, SAVE_NoError);       
	//	TArray<UObject*> ObjectsToSync;
	//	ObjectsToSync.Add(NewAsset);
	//	GEditor->SyncBrowserToObjects(ObjectsToSync);
	//	FrameTexture = NewAsset;
	//}
}

void UGaussDatasetCaptureTool::OnActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh)
{
	SourceActors.Reset();
	for (auto Item : NewSelection) {
		AActor* Actor = Cast<AActor>(Item);
		if (Actor == nullptr || Actor->HasAnyFlags(RF_Transient))
			continue;
		bool bHasPrimitive = false;
		for (auto Comp : Actor->GetComponents()) {
			if (Cast<UPrimitiveComponent>(Comp)) {
				bHasPrimitive = true;
			}
		}
		if (bHasPrimitive) {
			SourceActors.AddUnique(Actor);
		}
	}
	FBoxSphereBounds Bounds;
	Bounds.SphereRadius = 0;
	for (const auto& Actor : SourceActors) {
		if(Actor->Tags.Contains("GaussDatasetCaptureCamera"))
			continue;
		for (auto Comp : Actor->GetComponents()) {
			if (auto PrimitiveComponent = Cast<UPrimitiveComponent>(Comp)) {
				if (Bounds.SphereRadius <= 0) {
					Bounds = PrimitiveComponent->Bounds;
				}
				else {
					Bounds = Bounds + PrimitiveComponent->Bounds;
				}
			}
		}
	}
	if(Bounds.SphereRadius == 0)
		return;
	CurrentBounds = Bounds;
	if (CaptureActor) {
		USceneCaptureComponent2D* SceneCaptureComp = CaptureActor->GetCaptureComponent2D();
		SceneCaptureComp->ShowOnlyActors = SourceActors;
	}
	UpdateCameraMatrix();
}

void UGaussDatasetCaptureTool::UpdateCameraMatrix()
{
	UWorld* World = GetWorld();
	UStaticMesh* CameraMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/EditorMeshes/MatineeCam_SM.MatineeCam_SM"));
	TArray<AActor*> LastActors;
	UGameplayStatics::GetAllActorsOfClassWithTag(World, AStaticMeshActor::StaticClass(), "GaussDatasetCaptureCamera" , LastActors);
	for (auto Actor : LastActors) {
		Actor->Destroy();
	}
	float Radius = FMath::Max<FVector::FReal>(CurrentBounds.BoxExtent.Size(), 10.f);
	USceneCaptureComponent2D* SceneCaptureComp = CaptureActor->GetCaptureComponent2D();
	const float HalfFOVRadians = FMath::DegreesToRadians(SceneCaptureComp->FOVAngle / 2.0f);
	const float DistanceFromSphere = Radius / FMath::Tan(HalfFOVRadians) * 2;
	for (int i = 0; i < FrameXY; i++) {
		for (int j = 0; j < FrameXY; j++) {
			FVector Direction = UVtoPyramid(FVector2D(i / (double)FrameXY, j / (double)FrameXY));
			Direction.Normalize();
			FVector Position = CurrentBounds.Origin + Direction * DistanceFromSphere;
			FRotator Rotator = UKismetMathLibrary::FindLookAtRotation(Position, CurrentBounds.Origin);
			AStaticMeshActor* MeshActor = World->SpawnActor<AStaticMeshActor>();
			MeshActor->SetActorLocationAndRotation(Position, Rotator);
			MeshActor->SetFlags(EObjectFlags::RF_Transient);
			MeshActor->Tags.Add("GaussDatasetCaptureCamera");
			MeshActor->GetStaticMeshComponent()->SetStaticMesh(CameraMesh);
		}
	}
}
