#include "LODEditor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Engine/StaticMeshActor.h"
#include "LevelEditor.h"
#include "ProceduralContentProcessorLibrary.h"
#include "Selection.h"
#include "Engine/TextRenderActor.h"
#include "Components/TextRenderComponent.h"
#include "StaticMeshCompiler.h"
#include "StaticMeshAttributes.h"
#include "Engine/TextureRenderTarget.h"
#include "ProceduralMeshComponent.h"
#include "ProceduralMeshConversion.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Kismet/GameplayStatics.h"

ULODEditor::ULODEditor()
{

}

void ULODEditor::Activate()
{
	FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	OnActorSelectionChangedHandle = LevelEditor.OnActorSelectionChanged().AddUObject(this, &ULODEditor::OnActorSelectionChanged);
	TArray<UObject*> Objects;
	GEditor->GetSelectedActors()->GetSelectedObjects(Objects);
	OnActorSelectionChanged(Objects, true);
	StaticMeshes.Reset();
	for (auto MeshPath : StaticMeshesForConfig) {
		StaticMeshes.AddUnique(Cast<UStaticMesh>(MeshPath.TryLoad()));
	}
}

void ULODEditor::Deactivate()
{
	if (OnActorSelectionChangedHandle.IsValid()) {
		FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
		LevelEditor.OnActorSelectionChanged().Remove(OnActorSelectionChangedHandle);
		OnActorSelectionChangedHandle.Reset();
	}
	StaticMeshesForConfig.Reset();
	for (auto Mesh : StaticMeshes) {
		StaticMeshesForConfig.AddUnique(Mesh);
	}
	TryUpdateDefaultConfigFile();
}


void ULODEditor::ApplyMinLODForAll()
{
	for (auto StaticMesh : StaticMeshes) {
		StaticMesh->Modify();
		StaticMesh->SetMinLOD(MinLOD);
	}
}

void ULODEditor::RefreshPreviewMatrix()
{
	UWorld* World = GetWorld();
	TArray<AActor*> OutActors; 
	UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), OutActors);
	for (auto Actor : OutActors) {
		if (Actor && (Actor->Tags.Contains("LODPreviewActor") || Actor->Tags.Contains("LODPreviewTextActor"))) {
			Actor->Destroy();
		}
	}

	float YOffset = 0;
	TArray<AStaticMeshActor*> MeshActor;
	for (auto StaticMesh : StaticMeshes) {
		if (!StaticMesh) {
			continue;
		}
		const FBoxSphereBounds& Bound = StaticMesh->GetBounds();
		for (int Y = 0; Y < StaticMesh->GetNumLODs(); Y++) {
			float ScreenSize = UProceduralContentProcessorLibrary::GetLodScreenSize(StaticMesh, Y);
			float XOffset = UProceduralContentProcessorLibrary::GetLodDistance(StaticMesh, Y);
			AStaticMeshActor* PreviewActor = World->SpawnActor<AStaticMeshActor>(FVector(XOffset, YOffset, 0), FRotator(0, 0, 0));
			PreviewActor->GetStaticMeshComponent()->SetStaticMesh(StaticMesh);
			PreviewActor->GetStaticMeshComponent()->ForcedLodModel = Y + 1;
			PreviewActor->GetStaticMeshComponent()->SetForceDisableNanite(true);
			PreviewActor->SetActorLabel(FString::Printf(TEXT("LODPreviewActor[%s]:LOD%d"), *StaticMesh->GetName(), Y));
			PreviewActor->Tags.Add("LODPreviewActor");
			ATextRenderActor* TextActor = World->SpawnActor<ATextRenderActor>();
			TextActor->AttachToActor(PreviewActor, FAttachmentTransformRules::SnapToTargetIncludingScale);
			TextActor->SetActorRelativeLocation(FVector(0, 0, Bound.SphereRadius * 2 + 100));
			TextActor->SetActorRelativeRotation(FRotator(0, 180.0f, 0));
			TextActor->GetTextRender()->SetText(FText::FromString(
				FString::Printf(TEXT("LOD%d\nScreen Size:%f\nDistance:%d\nBound Sphere Radius:%d\nVertices:%d\nTriangles:%d")
					, Y
					, ScreenSize
					, (int)XOffset
					, (int)StaticMesh->GetBounds().SphereRadius
					, StaticMesh->GetNumVertices(Y)
					, StaticMesh->GetNumTriangles(Y))
			));
			TextActor->GetTextRender()->SetHorizontalAlignment(EHTA_Center);
			TextActor->GetTextRender()->SetWorldSize(100);
			TextActor->Tags.Add("LODPreviewTextActor");
			MeshActor.Add(PreviewActor);
		}
		YOffset += FMath::Max(Bound.SphereRadius * 2 + 200.0f, 1500.0f);
	}
	YOffset /= 2;
	for (auto Actor : MeshActor) {
		Actor->SetActorLocation(Actor->GetActorLocation() + FVector(10000, -YOffset, 0));
	}
}

void ULODEditor::GenerateLODForSelected()
{
	if (SelectedStaticMeshActor == nullptr)
			return;
	UWorld* World = GetWorld();
	UStaticMesh* StaticMesh = SelectedStaticMeshActor->GetStaticMeshComponent()->GetStaticMesh();
	if (StaticMesh == nullptr || StaticMesh->GetNumSourceModels() == 0 || SelectedStaticMeshLODChain.IsEmpty())
		return;
	bool bStaticMeshIsEdited = false;
	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (AssetEditorSubsystem->FindEditorForAsset(StaticMesh, false))
	{
		AssetEditorSubsystem->CloseAllEditorsForAsset(StaticMesh);
		bStaticMeshIsEdited = true;
	}
		
	const float FOV = 90.0f;
	const float FOVRad = FOV * (float)UE_PI / 360.0f;
	const FMatrix ProjectionMatrix = FPerspectiveMatrix(FOVRad, 1920, 1080, 0.01f);
		
	const float ScreenMultiple = FMath::Max(0.5f * ProjectionMatrix.M[0][0], 0.5f * ProjectionMatrix.M[1][1]);
	const float SphereRadius = StaticMesh->GetBounds().SphereRadius;
		
	StaticMesh->Modify();
	StaticMesh->SetNumSourceModels(1);
	StaticMesh->GetSourceModel(0).ReductionSettings = SelectedStaticMeshLODChain[0].ReductionSettings;
	StaticMesh->GetSourceModel(0).ScreenSize = SelectedStaticMeshLODChain[0].ScreenSize;

	if (SelectedStaticMeshLODChain[0].bUseDistance) {
		if (SelectedStaticMeshLODChain[0].Distance == 0) {
			StaticMesh->GetSourceModel(0).ScreenSize = 2.0f;
		}
		else {
			StaticMesh->GetSourceModel(0).ScreenSize = 2.0f * ScreenMultiple * SphereRadius / FMath::Max(1.0f, SelectedStaticMeshLODChain[0].Distance);
		}
	}
		
	int32 LODIndex = 1;
	for (; LODIndex < SelectedStaticMeshLODChain.Num(); ++LODIndex) {
		const FStaticMeshChainNode& ChainNode = SelectedStaticMeshLODChain[LODIndex];
		float ScreenSize = SelectedStaticMeshLODChain[LODIndex].ScreenSize;
		if (SelectedStaticMeshLODChain[LODIndex].bUseDistance) {
			if (SelectedStaticMeshLODChain[LODIndex].Distance == 0) {
				ScreenSize = 2.0f;
			}
			else {
				ScreenSize = 2.0f * ScreenMultiple * SphereRadius / FMath::Max(1.0f, SelectedStaticMeshLODChain[LODIndex].Distance);
			}
		}
		if (ChainNode.Type == EStaticMeshLODGenerateType::Reduce) {
			FStaticMeshSourceModel& SrcModel = StaticMesh->AddSourceModel();
			SrcModel.BuildSettings = SelectedStaticMeshLODChain[LODIndex].BuildSettings;
			SrcModel.ReductionSettings = SelectedStaticMeshLODChain[LODIndex].ReductionSettings;
			SrcModel.ScreenSize = ScreenSize;
		}
		else {
			if (!BP_Generate_ImposterSpritesActor) {
				if (BP_Generate_ImposterSprites) {
					BP_Generate_ImposterSpritesActor = World->SpawnActor(BP_Generate_ImposterSprites);
				}
				else {
					FNotificationInfo Info(FText::FromString(TEXT("ERROR! Please enable ImpostorBaker plugin")));
					Info.FadeInDuration = 2.0f;
					Info.ExpireDuration = 2.0f;
					Info.FadeOutDuration = 2.0f;
					FSlateNotificationManager::Get().AddNotification(Info);
					continue;
				}
			}
			ApplyImposterToLODChain(SelectedStaticMeshActor, BP_Generate_ImposterSpritesActor, LODIndex, ScreenSize);
		}
		
		// Stop when reaching maximum of supported LODs
		if (StaticMesh->GetNumSourceModels() == MAX_STATIC_MESH_LODS) {
			break;
		}
	}
	StaticMesh->bAutoComputeLODScreenSize = 0;
	StaticMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;
	StaticMesh->Build();
		
	StaticMesh->PostEditChange();
	StaticMesh->MarkPackageDirty();
	StaticMesh->WaitForPendingInitOrStreaming(true, true);
		
	FStaticMeshCompilingManager::Get().FinishAllCompilation();
		
	if (bStaticMeshIsEdited)
	{
		AssetEditorSubsystem->OpenEditorForAsset(StaticMesh);
	}
}

void ULODEditor::GenerateLODForAllByAuto()
{
	UWorld* World = GetWorld();
	TArray<AActor*> OutActors;
	TArray<AStaticMeshActor*> MeshActors;
	UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), OutActors);

	for (auto Actor : OutActors) {
		if (Actor && Actor->Tags.Contains("LODPreviewActor")) {
			if (auto StaticMeshActor = Cast<AStaticMeshActor>(Actor)) {
				if (StaticMeshActor->GetStaticMeshComponent()->ForcedLodModel == 1) {
					MeshActors.Add(StaticMeshActor);
				}
			}
		}
	}

	float AmountOfWork = MeshActors.Num();
	FScopedSlowTask RootTask(AmountOfWork, (NSLOCTEXT("Generate", "Generate", "Generate")));
	RootTask.MakeDialog();
	for (auto MeshActor : MeshActors) {
		SelectedStaticMeshActor = MeshActor;
		SelectedStaticMeshLODChain = AutoEvaluateLodChain(MeshActor);
		GenerateLODForSelected();
		RootTask.EnterProgressFrame(1, FText::FromString("Generate"));
	}
	RefreshPreviewMatrix();
}

TArray<FStaticMeshChainNode> ULODEditor::AutoEvaluateLodChain(AStaticMeshActor* MeshActor)
{
	TArray<FStaticMeshChainNode> LodChain;
	TArray<float> LodDistance = { 0, 3000, 6000, 10000, 18000, 30000 };
	TArray<float> LodPercentTriangles = { 1.0f, 0.5f, 0.25f, 0.1f, 0.05f, 0.001f };
	if (!MeshActor)
		return LodChain;
	UStaticMesh* StaticMesh = MeshActor->GetStaticMeshComponent()->GetStaticMesh();
	if (!StaticMesh)
		return LodChain;
	float SphereRadius = StaticMesh->GetBounds().SphereRadius;
	int SourceTriangles = StaticMesh->GetNumTriangles(0);
	int DesiredLodCount = 1;
	int DesiredImpostorResolution = 1024;
	if (SourceTriangles < 200) {
		DesiredLodCount = 1;
	}
	else if (SphereRadius > 1200) {
		DesiredLodCount = 4;
		DesiredImpostorResolution = 2048;
	}
	else if (SphereRadius > 800) {
		DesiredLodCount = 3;
		DesiredImpostorResolution = 2048;
	}
	else {
		DesiredLodCount = 3;
		DesiredImpostorResolution = 1024;
	}
	LodChain.SetNum(DesiredLodCount);
	ImposterSettings.Resolution = DesiredImpostorResolution;
	for (int i = 0; i < DesiredLodCount; i++) {
		//if (i != DesiredLodCount - 1) {
			LodChain[i].Type = EStaticMeshLODGenerateType::Reduce;
			LodChain[i].ReductionSettings.PercentTriangles = LodPercentTriangles[i];
		//}
		//else {
		//	LodChain[i].Type = EStaticMeshLODGenerateType::Imposter;
		//}
		LodChain[i].bUseDistance = true;
		LodChain[i].Distance = LodDistance[i];
	}
	return LodChain;
}

void ULODEditor::AutoEvaluateLodChainForSelected()
{
	SelectedStaticMeshLODChain = AutoEvaluateLodChain(SelectedStaticMeshActor);
}

void ULODEditor::OnActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh)
{
	SelectedStaticMeshLODChain.Reset();
	if (NewSelection.Num() != 1) {
		return;
	}
	AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>(NewSelection[0]);
	if (StaticMeshActor && StaticMeshActor->Tags.Contains("LODPreviewActor")) {
		SelectedStaticMeshActor = StaticMeshActor;
		RefreshLODChain();
	}
}

void ULODEditor::RefreshLODChain()
{
	SelectedStaticMeshLODChain.Reset();
	UStaticMesh* StaticMesh = SelectedStaticMeshActor->GetStaticMeshComponent()->GetStaticMesh();
	if (StaticMesh == nullptr)
		return ;
	SelectedStaticMeshLODChain.SetNum(StaticMesh->GetNumSourceModels());
	for (int i = 0; i < SelectedStaticMeshLODChain.Num(); i++) {
		SelectedStaticMeshLODChain[i].bEnableBuildSetting = bEnableBuildSetting;
		SelectedStaticMeshLODChain[i].bUseDistance = bUseDistance;
		SelectedStaticMeshLODChain[i].BuildSettings = StaticMesh->GetSourceModel(i).BuildSettings;
		SelectedStaticMeshLODChain[i].ReductionSettings = StaticMesh->GetSourceModel(i).ReductionSettings;
		SelectedStaticMeshLODChain[i].ScreenSize = StaticMesh->GetSourceModel(i).ScreenSize.GetValue();
		SelectedStaticMeshLODChain[i].Distance = UProceduralContentProcessorLibrary::GetLodDistance(StaticMesh, i);
	}
}

void ULODEditor::ApplyImposterToLODChain(AStaticMeshActor* InStaticMeshActor, AActor* ImposterSpritesActor, int TargetLODIndex, float ScreenSize)
{
	if (!InStaticMeshActor || !ImposterSpritesActor)
	return;
	UStaticMesh* StaticMesh = InStaticMeshActor->GetStaticMeshComponent()->GetStaticMesh();
	UClass* BPClass = ImposterSpritesActor->GetClass();
	if (FIntProperty* ResolutionProp = FindFProperty<FIntProperty>(BPClass, "Resolution")) {
		ResolutionProp->SetValue_InContainer(ImposterSpritesActor, ImposterSettings.Resolution);
	}
	if (FIntProperty* FrameXYProp = FindFProperty<FIntProperty>(BPClass, "Frame XY")) {
		FrameXYProp->SetValue_InContainer(ImposterSpritesActor, ImposterSettings.Resolution);
	}
	int32 ImpostorType = -1;
	UMaterialInterface* MaterialInterface = nullptr;
	if (FByteProperty* ImpostorTypeProp = FindFProperty<FByteProperty>(BPClass, "Impostor Type")) {
		ImpostorType = ImpostorTypeProp->GetUnsignedIntPropertyValue_InContainer(ImposterSpritesActor);
	}
	FObjectProperty* MaterialProp = nullptr;
	if (ImpostorType == 0) {
		MaterialProp = FindFProperty<FObjectProperty>(BPClass, "Full Sphere Material");
	}
	else if (ImpostorType == 1) {
		MaterialProp = FindFProperty<FObjectProperty>(BPClass, "Upper Hemisphere Material");
	}
	else if (ImpostorType == 2) {
		MaterialProp = FindFProperty<FObjectProperty>(BPClass, "Billboard Material");
	}
	if (MaterialProp) {
		if (ImposterSettings.ParentMaterial.IsValid()) {
			MaterialInterface = Cast<UMaterialInterface>(ImposterSettings.ParentMaterial.TryLoad());
			MaterialProp->SetObjectPropertyValue_InContainer(ImposterSpritesActor, MaterialInterface);
		}
		else {
			MaterialInterface = Cast<UMaterialInterface>(MaterialProp->GetObjectPropertyValue_InContainer(ImposterSpritesActor));
		}
	}
		
	if (UFunction* ClearRTFunc = BPClass->FindFunctionByName("1) Clear RTs")) {
		ImposterSpritesActor->ProcessEvent(ClearRTFunc, nullptr);
	}
	if (UFunction* RenderFramesFunc = BPClass->FindFunctionByName("2) RenderFrames")) {
		if (FObjectProperty* ActorProp = FindFProperty<FObjectProperty>(BPClass, "Static Mesh Actor")) {
			ActorProp->SetObjectPropertyValue_InContainer(ImposterSpritesActor, InStaticMeshActor);
			ImposterSpritesActor->UserConstructionScript();
		}
		ImposterSpritesActor->ProcessEvent(RenderFramesFunc, nullptr);
	}
	bool bNeedNewMesh = false;
	int32 MaterialIndex = 0;
	UMaterialInstanceConstant* MIC = FindObject<UMaterialInstanceConstant>(StaticMesh, TEXT("MIC_Impostor"));
	auto MICS = StaticMesh->GetStaticMaterials();
	if (MIC != nullptr) {
		if (StaticMesh->GetStaticMaterials().Contains(MIC)) {
			const auto& LastTriangles = StaticMesh->GetNumTriangles(StaticMesh->GetNumSourceModels() - 1);
			if (LastTriangles != 8) {
				MaterialIndex = StaticMesh->GetStaticMaterials().IndexOfByKey(MIC);
				bNeedNewMesh = true;
			}
		}
		else {
			MIC->ConditionalBeginDestroy();
			MIC = nullptr;
		}
	}
	if (MIC == nullptr) {
		UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
		Factory->InitialParent = MaterialInterface;
		MIC = Cast<UMaterialInstanceConstant>(Factory->FactoryCreateNew(UMaterialInstanceConstant::StaticClass(), StaticMesh, "MIC_Impostor", RF_Standalone | RF_Public, NULL, GWarn));
		MaterialIndex = StaticMesh->GetStaticMaterials().Add(FStaticMaterial(MIC));
		bNeedNewMesh = true;
	}
	FProperty* TargetMapsProp = FindFProperty<FProperty>(BPClass, "TargetMaps");
	TMap<uint32, UTextureRenderTarget*> TargetMaps;
	TargetMapsProp->GetValue_InContainer(ImposterSpritesActor, &TargetMaps);
	const uint32 BaseColorIndex = 0;
	const uint32 NormalIndex = 6;
	if (TargetMaps.IsEmpty()) {
		return;
	}
	UTextureRenderTarget2D* TextureRenderTarget2D = Cast<UTextureRenderTarget2D>(TargetMaps[BaseColorIndex]);
	UTexture* NewObj = TextureRenderTarget2D->ConstructTexture2D(MIC, "BaseColor", TextureRenderTarget2D->GetMaskedFlags() | RF_Public | RF_Standalone,
		static_cast<EConstructTextureFlags>(CTF_Default | CTF_SkipPostEdit), /*InAlphaOverride = */nullptr);
	NewObj->CompressionSettings = TextureCompressionSettings::TC_Default;
	NewObj->LODGroup = TextureGroup::TEXTUREGROUP_World;
	NewObj->MarkPackageDirty();
	NewObj->PostEditChange();
	MIC->SetTextureParameterValueEditorOnly(FName("BaseColor"), NewObj);

	TextureRenderTarget2D = Cast<UTextureRenderTarget2D>(TargetMaps[NormalIndex]);
	NewObj = TextureRenderTarget2D->ConstructTexture2D(MIC, "Normal", TextureRenderTarget2D->GetMaskedFlags() | RF_Public | RF_Standalone,
		static_cast<EConstructTextureFlags>(CTF_Default | CTF_AllowMips | CTF_SkipPostEdit), /*InAlphaOverride = */nullptr);
	NewObj->CompressionSettings = TextureCompressionSettings::TC_Default;
	NewObj->LODGroup = TextureGroup::TEXTUREGROUP_World;
	NewObj->MarkPackageDirty();
	NewObj->PostEditChange();
	MIC->SetTextureParameterValueEditorOnly(FName("Normal"), NewObj);

	if (FIntProperty* Prop = FindFProperty<FIntProperty>(BPClass, "FramesXYInternal")) {
		int FrameXY = Prop->GetSignedIntPropertyValue_InContainer(ImposterSpritesActor);
		MIC->SetScalarParameterValueEditorOnly(FName("FramesXY"), FrameXY);
	}
	if (FNumericProperty* Prop = FindFProperty<FNumericProperty>(BPClass, "Object Radius")) {
		double ObjectRadius = 0;
		Prop->GetValue_InContainer(ImposterSpritesActor, &ObjectRadius);
		MIC->SetScalarParameterValueEditorOnly(FName("Default Mesh Size"), ObjectRadius * 2);
	}
	if (FProperty* Prop = FindFProperty<FProperty>(BPClass, "Offset Vector")) {
		FVector OffsetVector;
		Prop->GetValue_InContainer(ImposterSpritesActor, &OffsetVector);
		MIC->SetVectorParameterValueEditorOnly(FName("Pivot Offset"), FLinearColor(OffsetVector));
	}

	if (bNeedNewMesh) {
		const int32 BaseLOD = 0;
		FStaticMeshSourceModel* SourceModel = nullptr;
		if (TargetLODIndex <= StaticMesh->GetNumSourceModels()) {
			SourceModel = &StaticMesh->GetSourceModel(TargetLODIndex);
		}
		else {
			TargetLODIndex = StaticMesh->GetNumSourceModels();
			SourceModel = &StaticMesh->AddSourceModel();
		}
		FMeshDescription& NewMeshDescription = *StaticMesh->CreateMeshDescription(TargetLODIndex);
		FStaticMeshAttributes(NewMeshDescription).Register();

		UProceduralMeshComponent* ProceduralMeshComp = ImposterSpritesActor->GetComponentByClass<UProceduralMeshComponent>();
		NewMeshDescription = BuildMeshDescription(ProceduralMeshComp);

		SourceModel->BuildSettings = StaticMesh->GetSourceModel(BaseLOD).BuildSettings;
		SourceModel->BuildSettings.bUseHighPrecisionTangentBasis = false;
		SourceModel->BuildSettings.bUseFullPrecisionUVs = false;
		SourceModel->BuildSettings.bRecomputeNormals = false;
		SourceModel->BuildSettings.bRecomputeTangents = false;
		SourceModel->BuildSettings.bRemoveDegenerates = true;
		SourceModel->BuildSettings.BuildScale3D = FVector(1, 1, 1);
		SourceModel->ScreenSize.Default = ScreenSize;

		StaticMesh->CommitMeshDescription(TargetLODIndex);
		FMeshSectionInfo Info = StaticMesh->GetSectionInfoMap().Get(TargetLODIndex, 0);
		Info.MaterialIndex = MaterialIndex;
		StaticMesh->GetSectionInfoMap().Set(TargetLODIndex, 0, Info);
	}
}

void ULODEditor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	const FName PropertyName = (PropertyChangedEvent.Property != NULL) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ULODEditor, bUseDistance) || PropertyName == GET_MEMBER_NAME_CHECKED(ULODEditor, bEnableBuildSetting)){
		RefreshLODChain();
	}
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ULODEditor, SelectedStaticMeshLODChain)) {
		for (auto& Node : SelectedStaticMeshLODChain) {
			Node.bEnableBuildSetting = bEnableBuildSetting;
			Node.bUseDistance = bUseDistance;
		}
		if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayAdd) {
			SelectedStaticMeshLODChain.Last();
		}
	}
}
