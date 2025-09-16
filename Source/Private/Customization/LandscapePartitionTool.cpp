#include "LandscapePartitionTool.h"
#include "LevelEditor.h"
#include "LandscapeConfigHelper.h"
#include "DesktopPlatformModule.h"
#include "LandscapeEdit.h"
#include "ImageUtils.h"
#include "Selection.h"

void ULandscapePartitionTool::Activate()
{
	FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	OnActorSelectionChangedHandle = LevelEditor.OnActorSelectionChanged().AddUObject(this, &ULandscapePartitionTool::OnActorSelectionChanged);
	TArray<UObject*> Objects;
	GEditor->GetSelectedActors()->GetSelectedObjects(Objects);
	OnActorSelectionChanged(Objects, true);
}

void ULandscapePartitionTool::Deactivate()
{
	if (OnActorSelectionChangedHandle.IsValid()) {
		FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
		LevelEditor.OnActorSelectionChanged().Remove(OnActorSelectionChangedHandle);
		OnActorSelectionChangedHandle.Reset();
	}
	TryUpdateDefaultConfigFile();
}

void ULandscapePartitionTool::Repartition()
{
	if (LandscapeActor) {
		UWorld* World = LandscapeActor->GetWorld();
		auto OldSize = LandscapeActor->GetGridSize();
		FLandscapeConfigChange NewConfig(ComponentNumSubSections, SubsectionSizeQuads, GridSize, ELandscapeResizeMode::Resample, false);
		TSet<AActor*> ActorsToDelete, ModifiedActors;
		FLandscapeConfigHelper::ChangeConfiguration(LandscapeActor->GetLandscapeInfo(), NewConfig, ActorsToDelete, ModifiedActors);
		//FLandscapeConfigHelper::PartitionLandscape(World, LandscapeActor->GetLandscapeInfo(), LandscapeGridSize);
	}
}

void ULandscapePartitionTool::OnActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh)
{
	if (NewSelection.Num() != 1) {
		return;
	}
	LandscapeActor = Cast<ALandscape>(NewSelection[0]);
}

void ULandscapePartitionTool::Export()
{
	if (!LandscapeActor) {
		return;
	}
	ULandscapeInfo* LandscapeInfo = LandscapeActor->GetLandscapeInfo();
	FIntRect ExportRegion;
	if (LandscapeInfo->GetLandscapeExtent(ExportRegion)) {
		TArray<FString> SaveFilenames;
		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
		bool bSaved = false;
		if (DesktopPlatform){
			DesktopPlatform->SaveFileDialog(
				FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
				NSLOCTEXT("", "SaveDialogTitle", "Save Landscape Texture").ToString(),
				FPaths::ProjectSavedDir(),
				LandscapeActor->GetActorLabel(),
				TEXT("Image |*.png|"),
				EFileDialogFlags::None,
				SaveFilenames
			);
			if (SaveFilenames.Num() == 1){
				FString BaseFilename = SaveFilenames[0];
				FString Directory;
				FString Filename;
				FString Extension;
				FPaths::Split(BaseFilename, Directory, Filename, Extension);
				for (int i = 0; i < LandscapeActor->GetLayerCount(); i++) {
					const FLandscapeLayer* Layer = LandscapeActor->GetLayerConst(i);
					FString ExportFilename = FString::Printf(
						TEXT("%s_%s.%s"),
						*Filename,
						*Layer->Name.ToString().Replace(TEXT(" "), TEXT("_")),
						*Extension
					);
					FScopedSetLandscapeEditingLayer Scope(LandscapeActor, Layer->Guid);
					LandscapeInfo->ExportHeightmap(FPaths::Combine(Directory, ExportFilename));
				}
				for (auto Layer : LandscapeInfo->Layers) {
					FString ExportFilename = FString::Printf(
						TEXT("%s_%s.%s"),
						*Filename,
						*Layer.LayerName.ToString().Replace(TEXT(" "), TEXT("_")),
						*Extension
					);
					LandscapeInfo->ExportLayer(Layer.LayerInfoObj, FPaths::Combine(Directory, ExportFilename));
				}
			}
		}
	}
}

void ULandscapePartitionTool::Import()
{
	if (!LandscapeActor) {
		return;
	}
	TArray<FString> ImportFilePaths;
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform) {
		bool bSuccess = DesktopPlatform->OpenFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			NSLOCTEXT("", "SaveDialogTitle", "Save Landscape Texture").ToString(),
			"",
			"",                      
			TEXT("Image |*.png|"),               
			EFileDialogFlags::Multiple,
			ImportFilePaths
		);
	}

	ULandscapeInfo* LandscapeInfo = LandscapeActor->GetLandscapeInfo();

	for (auto ImportFilename: ImportFilePaths) {
		FString Directory;
		FString Filename;
		FString Extension;
		FPaths::Split(ImportFilename, Directory, Filename, Extension);
		for (int i = 0; i < LandscapeActor->GetLayerCount(); i++) {
			const FLandscapeLayer* Layer = LandscapeActor->GetLayerConst(i);
			FString NormalizedLayerName = Layer->Name.ToString().Replace(TEXT(" "), TEXT("_"));
			if (ImportFilename.Contains(NormalizedLayerName)) {
				FImage Image;
				FImageUtils::LoadImage(*ImportFilename, Image);
				if (Image.Format != ERawImageFormat::Type::G16) {
					break;
				}

				FIntRect TargetResolution;
				LandscapeInfo->GetLandscapeExtent(TargetResolution);
				if (Image.SizeX != TargetResolution.Width() + 1 || Image.SizeY != TargetResolution.Height() + 1) {
					FImage ScaledImage;
					Image.ResizeTo(ScaledImage, TargetResolution.Width() + 1, TargetResolution.Height() + 1, ERawImageFormat::Type::G16, Image.GammaSpace);
					Image = ScaledImage;
				}
				{
					FScopedSetLandscapeEditingLayer Scope(LandscapeActor, Layer->Guid, [&] {
						LandscapeActor->RequestLayersContentUpdate(ELandscapeLayerUpdateMode::Update_Heightmap_All);
					});
					FScopedTransaction Transaction(NSLOCTEXT("Landscape", "Undo_ImportHeightmap", "Importing Landscape Heightmap"));
					FHeightmapAccessor<false> HeightmapAccessor(LandscapeInfo);
					HeightmapAccessor.SetData(TargetResolution.Min.X, TargetResolution.Min.Y, TargetResolution.Max.X, TargetResolution.Max.Y, (uint16_t*)Image.RawData.GetData());
				}
				break;
			}
		}
	}

	LandscapeInfo->ForceLayersFullUpdate();
}
