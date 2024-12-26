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
#include "ImageWriteBlueprintLibrary.h"

void UGaussDatasetCaptureTool::Activate()
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
		SceneCaptureComp->bCaptureEveryFrame = false;
		SceneCaptureComp->bCaptureOnMovement = false;
		SceneCaptureComp->TextureTarget = CaptureRT;
		SceneCaptureComp->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		//SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("Lighting"), false });
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

void UGaussDatasetCaptureTool::Capture()
{
	USceneCaptureComponent2D* SceneCaptureComp = CaptureActor->GetCaptureComponent2D();
	SceneCaptureComp->CaptureScene();
	TArray<FColor> FullColors;
	ETextureRenderTargetFormat RenderTargetFormat = CaptureRT->RenderTargetFormat;
	FTextureRenderTargetResource* RenderTargetResource = CaptureRT->GameThread_GetRenderTargetResource();
	FReadSurfaceDataFlags ReadPixelFlags(RCM_MinMax);
	FIntRect IntRegion(0, 0, CaptureRT->SizeX, CaptureRT->SizeY);
	RenderTargetResource->ReadPixels(FullColors, ReadPixelFlags, IntRegion);
	FImageView ImageView(FullColors.GetData(), CaptureRT->SizeX, CaptureRT->SizeY);
	FImageUtils::SaveImageByExtension(TEXT("F:/gaussian-splatting/dataset_ue/Images/test.png"), ImageView);
}

void UGaussDatasetCaptureTool::CreateAsset()
{
	FImageWriteOptions WriteOptions;
	WriteOptions.Format = EDesiredImageFormat::PNG;
	WriteOptions.bOverwriteFile = true;
	WriteOptions.bAsync = false;
	WriteOptions.CompressionQuality = 100.0f;
	UImageWriteBlueprintLibrary::ExportToDisk(FrameTexture, "F:/gaussian-splatting/dataset_ue/Images/test.png", WriteOptions);
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
	//CurrentBounds = Bounds;
	if (CaptureActor) {
		USceneCaptureComponent2D* SceneCaptureComp = CaptureActor->GetCaptureComponent2D();
		SceneCaptureComp->ShowOnlyActors = SourceActors;
	}
}