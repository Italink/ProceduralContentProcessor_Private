#include "FrameAnimCaptureTool.h"
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
#include "../../../../../../../Source/Runtime/AssetRegistry/Public/AssetRegistry/AssetRegistryModule.h"

void UFrameAnimCaptureTool::Activate()
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
			if (Actor->Tags.Contains("FrameAnimCapture")) {
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
		SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("Lighting"), false });
		SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("Atmosphere"), false });
		SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("Bloom"), false });
		SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("Fog"), false });
		SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("DynamicShadows"), false });
		SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("BSP"), false });
		SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("Cloud"), false });
		SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("Decals"), false });
		SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("DepthOfField"), false });
		SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("VolumetricFog"), false });
		SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("Refraction"), false });
		SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("ScreenSpaceReflections"), false });
		SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("AmbientCubemap"), false });
		SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("ReflectionEnvironment"), false });
		SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("EyeAdaptation"), false });
		SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("ColorGrading"), false });
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
	FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	OnActorSelectionChangedHandle = LevelEditor.OnActorSelectionChanged().AddUObject(this, &UFrameAnimCaptureTool::OnActorSelectionChanged);
	TArray<UObject*> Objects;
	GEditor->GetSelectedActors()->GetSelectedObjects(Objects);
	OnActorSelectionChanged(Objects, true);
}

void UFrameAnimCaptureTool::Deactivate()
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

void UFrameAnimCaptureTool::Tick(const float InDeltaTime)
{
	UWorld* World = GetWorld();
	if (!SourceActors.IsEmpty()) {
		FBoxSphereBounds BoundsWithScale = CurrentBounds;
		BoundsWithScale.BoxExtent *= BoundsScaleFactor;
		DrawDebugBox(World, BoundsWithScale.Origin, BoundsWithScale.BoxExtent * BoundsScaleFactor, FColor::Green, false, -1.0f, 0, 10.0f);
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
		TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule.GetFirstLevelEditor();
		if (LevelEditor) {
			FIntPoint ViewportSize = LevelEditor->GetActiveViewportSize().IntPoint();
			if (CaptureRT->SizeX != ViewportSize.X || CaptureRT->SizeY != ViewportSize.Y) {
				UKismetRenderingLibrary::ResizeRenderTarget2D(CaptureRT, ViewportSize.X, ViewportSize.Y);
			}
			FVector2D ScreenCenter = UProceduralContentProcessorLibrary::ProjectWorldToScreen(BoundsWithScale.Origin, false);
			FVector2D ScreenPositions[] = {
				UProceduralContentProcessorLibrary::ProjectWorldToScreen(BoundsWithScale.Origin + FVector(-BoundsWithScale.BoxExtent.X, -BoundsWithScale.BoxExtent.Y, -BoundsWithScale.BoxExtent.Z), true),
				UProceduralContentProcessorLibrary::ProjectWorldToScreen(BoundsWithScale.Origin + FVector(-BoundsWithScale.BoxExtent.X, -BoundsWithScale.BoxExtent.Y, +BoundsWithScale.BoxExtent.Z), true),
				UProceduralContentProcessorLibrary::ProjectWorldToScreen(BoundsWithScale.Origin + FVector(-BoundsWithScale.BoxExtent.X, +BoundsWithScale.BoxExtent.Y, -BoundsWithScale.BoxExtent.Z), true),
				UProceduralContentProcessorLibrary::ProjectWorldToScreen(BoundsWithScale.Origin + FVector(-BoundsWithScale.BoxExtent.X, +BoundsWithScale.BoxExtent.Y, +BoundsWithScale.BoxExtent.Z), true),
				UProceduralContentProcessorLibrary::ProjectWorldToScreen(BoundsWithScale.Origin + FVector(+BoundsWithScale.BoxExtent.X, -BoundsWithScale.BoxExtent.Y, -BoundsWithScale.BoxExtent.Z), true),
				UProceduralContentProcessorLibrary::ProjectWorldToScreen(BoundsWithScale.Origin + FVector(+BoundsWithScale.BoxExtent.X, -BoundsWithScale.BoxExtent.Y, +BoundsWithScale.BoxExtent.Z), true),
				UProceduralContentProcessorLibrary::ProjectWorldToScreen(BoundsWithScale.Origin + FVector(+BoundsWithScale.BoxExtent.X, +BoundsWithScale.BoxExtent.Y, -BoundsWithScale.BoxExtent.Z), true),
				UProceduralContentProcessorLibrary::ProjectWorldToScreen(BoundsWithScale.Origin + FVector(+BoundsWithScale.BoxExtent.X, +BoundsWithScale.BoxExtent.Y, +BoundsWithScale.BoxExtent.Z), true),
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
		if (BeginFrameCounter != -1) {
			int TargetFrame = (GFrameCounterRenderThread - BeginFrameCounter) / FrameStep;
			if(CurrentFrameIndex != TargetFrame)
				return;
			if (CurrentFrameIndex < FrameCount && CaptureActor) {
				USceneCaptureComponent2D* SceneCaptureComp = CaptureActor->GetCaptureComponent2D();
				SceneCaptureComp->CaptureScene();
				FIntPoint BlockOffset;
				BlockOffset.X = CurrentBlockTextureSize.X * (CurrentFrameIndex % CurrentBlockCellSize.X);
				BlockOffset.Y = CurrentBlockTextureSize.Y * (CurrentFrameIndex / CurrentBlockCellSize.X);
				if (bPaddingToPowerOfTwo) {
					BlockOffset.X += (CurrentBlockTextureSize.X - CurrentScreenBoundsSize.X) / 2;
					BlockOffset.Y += (CurrentBlockTextureSize.Y - CurrentScreenBoundsSize.Y) / 2;
				}
				TArray<FColor> FullColors;
				ETextureRenderTargetFormat RenderTargetFormat = CaptureRT->RenderTargetFormat;
				FTextureRenderTargetResource* RenderTargetResource = CaptureRT->GameThread_GetRenderTargetResource();
				FReadSurfaceDataFlags ReadPixelFlags(RCM_MinMax);
				FIntRect IntRegion(CurrentScreenBounds.Min.IntPoint().ComponentMax(FIntPoint(0, 0)), CurrentScreenBounds.Max.IntPoint().ComponentMin(FIntPoint(CaptureRT->SizeX, CaptureRT->SizeY)));
				RenderTargetResource->ReadPixels(FullColors, ReadPixelFlags, IntRegion);
				for (int i = 0; i < IntRegion.Width(); i++) {
					for (int j = 0; j < IntRegion.Height(); j++) {
						FColor SampleColor = FullColors[j * IntRegion.Width() + i];
						SampleColor.A = 255 - SampleColor.A;
						FrameTextureData[(BlockOffset.Y + j) * CurrentFrameTextureSize.X + (BlockOffset.X + i)] = SampleColor;
					}
				}
				CurrentFrameIndex++;
			}
			else {
				FCreateTexture2DParameters Params;
				Params.CompressionSettings = TC_Default;
				Params.bUseAlpha = true;
				FrameTexture = FImageUtils::CreateTexture2D(CurrentFrameTextureSize.X, CurrentFrameTextureSize.Y, FrameTextureData, GetTransientPackage(), "FrameAnimTexture", EObjectFlags::RF_NoFlags, Params);
				BeginFrameCounter = -1;
				CurrentFrameIndex = -1;
			}
		}
	}
}

void UFrameAnimCaptureTool::Capture()
{
	CurrentScreenBoundsSize = FIntPoint(CurrentScreenBounds.Max.X - CurrentScreenBounds.Min.X, CurrentScreenBounds.Max.Y - CurrentScreenBounds.Min.Y);
	if (CurrentScreenBoundsSize.X <= 0 || CurrentScreenBoundsSize.Y <= 0) {
		return;
	}
	CurrentBlockTextureSize = CurrentScreenBoundsSize;
	CurrentBlockCellSize.X = FMath::RoundToInt(FMath::Sqrt((float)FrameCount));
	CurrentBlockCellSize.Y = FMath::CeilToInt(FrameCount/(float)CurrentBlockCellSize.X);
	CurrentFrameTextureSize = CurrentScreenBoundsSize * CurrentBlockCellSize;
	if (bPaddingToPowerOfTwo) {
		CurrentFrameTextureSize.X = FMath::RoundUpToPowerOfTwo(CurrentFrameTextureSize.X);
		CurrentFrameTextureSize.Y = FMath::RoundUpToPowerOfTwo(CurrentFrameTextureSize.Y);
		CurrentBlockTextureSize.X = CurrentFrameTextureSize.X / CurrentBlockCellSize.X;
		CurrentBlockTextureSize.Y = CurrentFrameTextureSize.Y / CurrentBlockCellSize.Y;
	}
	FrameTextureData.Reset();
	FrameTextureData.SetNumZeroed(CurrentFrameTextureSize.X * CurrentFrameTextureSize.Y);
	BeginFrameCounter = GFrameCounterRenderThread;
	CurrentFrameIndex = 0;
}

void UFrameAnimCaptureTool::CreateAsset()
{
	if (!FrameTexture) {
		return;
	}
	FSaveAssetDialogConfig SaveAssetDialogConfig;
	SaveAssetDialogConfig.DefaultPath = LastSavePath;
	SaveAssetDialogConfig.DefaultAssetName = CurrentName;
	SaveAssetDialogConfig.AssetClassNames.Add(UTexture2D::StaticClass()->GetClassPathName());
	SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::AllowButWarn;
	SaveAssetDialogConfig.DialogTitleOverride = FText::FromString("Save As");

	const FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	const FString SaveObjectPath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);
	if (SaveObjectPath.IsEmpty())
	{
		FNotificationInfo NotifyInfo(FText::FromString("Path is empty"));
		NotifyInfo.ExpireDuration = 5.0f;
		FSlateNotificationManager::Get().AddNotification(NotifyInfo);
		return;
	}
	const FString PackagePath = FPackageName::ObjectPathToPackageName(SaveObjectPath);
	const FString AssetName = FPaths::GetBaseFilename(PackagePath, true);
	LastSavePath = PackagePath;
	if (!AssetName.IsEmpty()) {
		UPackage* NewPackage = CreatePackage(*PackagePath);
		UTexture2D* NewAsset = DuplicateObject<UTexture2D>(FrameTexture, NewPackage, *AssetName);
		NewAsset->SetFlags(RF_Public | RF_Standalone);
		FAssetRegistryModule::AssetCreated(NewAsset);
		FPackagePath NewPackagePath = FPackagePath::FromPackageNameChecked(NewPackage->GetName());
		FString PackageLocalPath = NewPackagePath.GetLocalFullPath();
		UPackage::SavePackage(NewPackage, NewAsset, RF_Public | RF_Standalone, *PackageLocalPath, GError, nullptr, false, true, SAVE_NoError);       
		TArray<UObject*> ObjectsToSync;
		ObjectsToSync.Add(NewAsset);
		GEditor->SyncBrowserToObjects(ObjectsToSync);
		FrameTexture = NewAsset;
	}
}

void UFrameAnimCaptureTool::OnActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh)
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
	CurrentBounds = Bounds;
	if (CaptureActor) {
		USceneCaptureComponent2D* SceneCaptureComp = CaptureActor->GetCaptureComponent2D();
		SceneCaptureComp->ShowOnlyActors = SourceActors;
	}
}