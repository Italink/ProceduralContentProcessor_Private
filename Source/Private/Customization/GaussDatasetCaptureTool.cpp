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
#include "Interfaces/IPluginManager.h"

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
		//SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("Lighting"), false });
		SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("Atmosphere"), false });
		//SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("Bloom"), false });
		SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("Fog"), false });
		//SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("DynamicShadows"), false });
		//SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("BSP"), false });
		SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("Cloud"), false });
		SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("Decals"), false });
		SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("DepthOfField"), false });
		SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("VolumetricFog"), false });
		//SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("Refraction"), false });
		//SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("ScreenSpaceReflections"), false });
		SceneCaptureComp->ShowFlagSettings.Add(FEngineShowFlagsSetting{ TEXT("AmbientCubemap"), false });
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

FVector TransposeMultiply(const FMatrix& R, const FVector& T)
{
	FMatrix TransposedR = R.GetTransposed();
	FVector4d HomogeneousT(T.X, T.Y, T.Z, 1.0f);
	FVector Result4D = TransposedR.TransformFVector4(HomogeneousT);
	FVector Result3D(Result4D.X, Result4D.Y, Result4D.Z);
	Result3D = -Result3D;
	return Result3D;
}

void UGaussDatasetCaptureTool::Capture()
{
	float Radius = FMath::Max<FVector::FReal>(CurrentBounds.BoxExtent.Size(), 10.f);
	USceneCaptureComponent2D* SceneCaptureComp = CaptureActor->GetCaptureComponent2D();

	const double HalfFOVRadians = FMath::DegreesToRadians(SceneCaptureComp->FOVAngle / 2.0);
	const double DistanceFromSphere = Radius / FMath::Tan(HalfFOVRadians) * 2 * CaptureDistanceScale;
	const double FocalLength = CaptureRT->SizeX / (2 * FMath::Tan(HalfFOVRadians));

	const FString PluginDir = IPluginManager::Get().FindPlugin(TEXT("ProceduralContentProcessor"))->GetBaseDir();
	const FString GSDir = PluginDir / "PluginDir";
	const FString DatabaseImagesDir = GSDir / "database" / "images";
	const FString DatabaseSparseDir = GSDir / "database" / "sparse" / "0";
	const FString ImagesFilePath = GSDir / "database" / "text" / "images.txt";
	const FString CamerasFilePath = GSDir / "database" / "text" / "cameras.txt";
	const FString Points3DFilePath = GSDir / "database" / "text" / "points3D.txt";
	const FString ImageRefPosFilePath = GSDir / "database" / "text" / "imageRefPos.txt";

	FString ImagesFileContent =
		"# Image list with two lines of data per image:\n"
		"# IMAGE_ID, QW, QX, QY, QZ, TX, TY, TZ, CAMERA_ID, NAME\n"
		"# POINTS2D[] as (X, Y, POINT3D_ID)\n";
	ImagesFileContent += FString::Printf(TEXT("# Number of images: %d, mean observations per image: %d\n"), FrameXY * FrameXY, FrameXY * FrameXY);

	FString CamerasFileContent =
		"# Camera list with one line of data per camera:\n"
		"#   CAMERA_ID, MODEL, WIDTH, HEIGHT, PARAMS[]\n";
	CamerasFileContent += FString::Printf(TEXT("# Number of cameras : %d\n"), FrameXY * FrameXY);

	FString ImageRefPosFileContent = "";

	for (int i = 0; i < FrameXY; i++) {
		for (int j = 0; j < FrameXY; j++) {
			int FrameID = i * FrameXY +j + 1;
			FVector Direction = UVtoPyramid(FVector2D(i / (double)(FrameXY - 1), j / (double)(FrameXY - 1)));
			Direction.Normalize();
			FVector PositionOffset = Direction * DistanceFromSphere;
			FVector Position = CurrentBounds.Origin + PositionOffset;
			FQuat Quat = UKismetMathLibrary::FindLookAtRotation(Position, CurrentBounds.Origin).Quaternion();
			SceneCaptureComp->SetWorldLocationAndRotation(Position, Quat);
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
			FString ImageFileName = FString::Printf(TEXT("image%04d.png"), FrameID);
			FImageUtils::SaveImageByExtension(*(DatabaseImagesDir / ImageFileName), ImageView);

			FVector ColmapPosition = FVector(PositionOffset.X, PositionOffset.Y, PositionOffset.Z) / 100.0;
			FQuat ColmapQuat = /*FRotator(45.0, 0.0 , 0.0).Quaternion() **/ Quat * FRotator(180, 90, -90).Quaternion();

			FMatrix TransposedR = ColmapQuat.ToMatrix().GetTransposed();
			FVector4d HomogeneousT(ColmapPosition, 1.0f);
			FVector Result3D = TransposedR.TransformFVector4(HomogeneousT);
			Result3D = -Result3D;

			FQuat TransposedQuat = TransposedR.ToQuat();
		
			UE_LOG(LogTemp, Warning, TEXT("%d %d : %s"), i, j, *Quat.Rotator().ToString());

			ImagesFileContent += FString::Printf(TEXT("%d %lf %lf %lf %lf %lf %lf %lf %d %s\n\n"),
				FrameID,
				TransposedQuat.W,
				TransposedQuat.X,
				TransposedQuat.Y,
				TransposedQuat.Z,
				Result3D.X,
				Result3D.Y ,
				Result3D.Z,
				FrameID,
				*ImageFileName
				);

			CamerasFileContent += FString::Printf(TEXT("%d %s %d %d %lf %lf %lf\n"),
				FrameID,
				TEXT("SIMPLE_PINHOLE"),
				CaptureRT->SizeX,
				CaptureRT->SizeY,
				FocalLength,
				CaptureRT->SizeX / 2.0,
				CaptureRT->SizeY / 2.0
			);

			ImageRefPosFileContent += FString::Printf(TEXT("%s %lf %lf %lf\n"),
				*ImageFileName,
				ColmapPosition.X,
				ColmapPosition.Z,
				ColmapPosition.Y
			);
		}
	}

	if (FFileHelper::SaveStringToFile(ImagesFileContent, *ImagesFilePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("Successfully created and wrote to %s"), *ImagesFilePath);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create or write to %s"), *ImagesFilePath);
	}

	if (FFileHelper::SaveStringToFile(CamerasFileContent, *CamerasFilePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("Successfully created and wrote to %s"), *CamerasFilePath);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create or write to %s"), *CamerasFilePath);
	}

	if (FFileHelper::SaveStringToFile(FString(), *Points3DFilePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("Successfully created and wrote to %s"), *Points3DFilePath);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create or write to %s"), *Points3DFilePath);
	}

	if (FFileHelper::SaveStringToFile(ImageRefPosFileContent, *ImageRefPosFilePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("Successfully created and wrote to %s"), *ImageRefPosFilePath);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create or write to %s"), *ImageRefPosFilePath);
	}
}

void UGaussDatasetCaptureTool::BrowseToDatabase()
{
	const FString PluginDir = IPluginManager::Get().FindPlugin(TEXT("ProceduralContentProcessor"))->GetBaseDir();
	const FString DatabaseDir = PluginDir / "database" ;
	if (!IFileManager::Get().DirectoryExists(*DatabaseDir))
		return;
	FPlatformProcess::ExploreFolder(*FPaths::GetPath(DatabaseDir));
}

void UGaussDatasetCaptureTool::ReconstructSparseModel()
{
	const FString PluginDir = IPluginManager::Get().FindPlugin(TEXT("ProceduralContentProcessor"))->GetBaseDir();
	const FString DatabaseDir = PluginDir / "database" / "sparse" / "0";

	FString Command = 

	int32 ReturnCode = -1;
	void* PipeRead = nullptr;
	void* PipeWrite = nullptr;
	verify(FPlatformProcess::CreatePipe(PipeRead, PipeWrite));
	FProcHandle ProcessHandle = FPlatformProcess::CreateProc(TEXT("C:\\Program Files (x86)\\Microsoft Visual Studio\\Shared\\Python39_64\\python.exe"), TEXT("F:\\UnrealProjects\\ItaDev\\Plugins\\ProceduralContentProcessor\\Scripts\\ColmapHelper.py"), true, true, true, nullptr, 0, nullptr, PipeWrite, nullptr, nullptr);
	if (ProcessHandle.IsValid())
	{
		FPlatformProcess::Sleep(0.01f);
		while (FPlatformProcess::IsProcRunning(ProcessHandle))
		{
			TArray<uint8> BinaryData;
			FPlatformProcess::ReadPipeToArray(PipeRead, BinaryData);
			if(!BinaryData.IsEmpty()){
				UE_LOG(LogTemp, Log, TEXT("%s"), *FString(UTF8_TO_TCHAR(BinaryData.GetData())));
			}
		}
		TArray<uint8> BinaryData;
		FPlatformProcess::ReadPipeToArray(PipeRead, BinaryData);
		if (!BinaryData.IsEmpty()) {
			UE_LOG(LogTemp, Log, TEXT("%s"), *FString(UTF8_TO_TCHAR(BinaryData.GetData())));
		}
		FPlatformProcess::GetProcReturnCode(ProcessHandle, &ReturnCode);
		if (ReturnCode != 0)
		{
			UE_LOG(LogTemp, Error, TEXT("DumpToFile: ReturnCode=%d"), ReturnCode);
		}
		FPlatformProcess::CloseProc(ProcessHandle);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to launch 'git cat-file'"));
	}
	FPlatformProcess::ClosePipe(PipeRead, PipeWrite);
}

void UGaussDatasetCaptureTool::OnActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh)
{
	USceneCaptureComponent2D* SceneCaptureComp = CaptureActor->GetCaptureComponent2D();
	TArray<TObjectPtr<AActor>> NewActors;
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
			NewActors.AddUnique(Actor);
		}
	}
	FBoxSphereBounds Bounds;
	Bounds.SphereRadius = 0;
	for (const auto& Actor : NewActors) {
		if(Actor->Tags.Contains("GaussDatasetCaptureCamera") || Actor == CaptureActor)
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
	if (NewSelection.Num() == 1) {
		AActor* Actor = Cast<AActor>(NewSelection[0]);
		if (Actor && Actor->Tags.Contains("GaussDatasetCaptureCamera")) {
			SceneCaptureComp->bCaptureEveryFrame = true;
			SceneCaptureComp->SetWorldTransform(Actor->GetActorTransform());
		}
	}
	if(Bounds.SphereRadius == 0)
		return;
	CurrentBounds = Bounds;
	SourceActors = NewActors;
	if (CaptureActor) {
		SceneCaptureComp->ShowOnlyActors = SourceActors;
		SceneCaptureComp->bCaptureEveryFrame = false;
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
	const float DistanceFromSphere = Radius / FMath::Tan(HalfFOVRadians) * 2 * CaptureDistanceScale;
	for (int i = 0; i < FrameXY; i++) {
		for (int j = 0; j < FrameXY; j++) {
			int FrameID = i * FrameXY + j + 1;
			FVector Direction = UVtoPyramid(FVector2D(i / (double)(FrameXY - 1), j / (double)(FrameXY - 1)));
			Direction.Normalize();
			FVector Position = CurrentBounds.Origin + Direction * DistanceFromSphere;
			FRotator Rotator = UKismetMathLibrary::FindLookAtRotation(Position, CurrentBounds.Origin);
			AStaticMeshActor* MeshActor = World->SpawnActor<AStaticMeshActor>();
			MeshActor->SetMobility(EComponentMobility::Movable);
			MeshActor->AttachToActor(CaptureActor, FAttachmentTransformRules::KeepWorldTransform);
			MeshActor->SetActorLocationAndRotation(Position, Rotator);
			MeshActor->SetFlags(EObjectFlags::RF_Transient);
			MeshActor->Tags.Add("GaussDatasetCaptureCamera");
			MeshActor->SetActorLabel(FString::Printf(TEXT("CaptureCamera%d[%d_%d]"), FrameID, i, j));
			MeshActor->GetStaticMeshComponent()->SetStaticMesh(CameraMesh);
		}
	}
}

void UGaussDatasetCaptureTool::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UGaussDatasetCaptureTool, FrameXY)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UGaussDatasetCaptureTool, CaptureDistanceScale)
		) {
		UpdateCameraMatrix();
	}
}
