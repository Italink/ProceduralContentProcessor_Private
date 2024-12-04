//#pragma once
//
//#include "ProceduralContentProcessor.h"
//#include "Components/DirectionalLightComponent.h"
//#include "ImpostorCaptureTool.generated.h"
//
//UENUM()
//enum class EImpostorCaptureChannel {
//	BaseColor,
//};
//
//UENUM()
//enum class EImpostorType {
//	FullSphereView,
//	UpperHemisphereOnly,
//	TraditionalBillboard,
//};
//
//
//UCLASS(EditInlineNew, CollapseCategories, config = ProceduralContentProcessor, defaultconfig, Category = "Model", meta = (DisplayName = "Impostor Capture Tool"))
//class PROCEDURALCONTENTPROCESSOR_API UImpostorCaptureTool: public UProceduralWorldProcessor {
//	GENERATED_BODY()
//protected:
//	virtual TSharedPtr<SWidget> BuildWidget() override;
//private:
//	void SetupRTAndSaveList();
//	void ClearRenderTargets();
//
//	TArray<TObjectPtr<AActor>> CaptureActors;
//	int32 FrameXY = 16;
//	int32 FrameXYInternal = 16;
//	int32 CurFrame = 0;
//	TObjectPtr<UTextureRenderTarget2D> CurrentTargetRT;
//	EImpostorCaptureChannel CurrentChannel = EImpostorCaptureChannel::BaseColor;
//	bool bUseDistanceFieldAlpha = true;
//	TArray<TObjectPtr<UDirectionalLightComponent>> Lights;
//	TArray<EImpostorCaptureChannel> ChannelsToRender;
//	EImpostorType ImpostorType;
//	TArray<EImpostorCaptureChannel> ChannelsToSave;
//	TMap<EImpostorCaptureChannel, TObjectPtr<UTexture2D>> SavedTextures;
//
//	bool bCombineCustomlightingAndBasecolor;
//	bool bCombineNormalAndDepth = true;
//	bool bPreviewProcMesh = true;
//	bool bUseMeshCutout = true;
//	bool Orthographic = true;
//	float CameraDistance = 1000.0f;
//	int32 Resolution = 2048;
//	int32 SceneCaptureResolution = 512;
//	int32 SceneCaptureMips = 0;
//	int32 CutoutMipTarget = 0;
//	int32 CheckTargetMipSize = 0;
//	int32 DFMipTarget = 0;
//	float BillboardTopOffset = 256.0f;
//	int32 BillboardTopOffsetCenter = 512.0f;
//
//	TObjectPtr<UMaterialInstanceDynamic> ResampleMID;
//	TObjectPtr<UMaterialInstanceDynamic> SampleFrameMID;
//	TObjectPtr<UMaterialInstanceDynamic> SampleFrame_DFAlphaMID;
//	TObjectPtr<UMaterialInstanceDynamic> BaseColorCustomLightMID;
//	TObjectPtr<UMaterialInstanceDynamic> CombineNormalDepthMID;
//	TObjectPtr<UMaterialInstanceDynamic> AddAlphasMID;
//
//	float CustomLightingPower = 1.0f;
//	float CustomLightingOpacity = 0.0f;
//	bool PreviewCustomLighting = false;
//	int32 LightingGridXY = 4;
//	float UpwardBias = 0.75f;
//	float DirLightBrightness = 3.14f;
//	float CustomSkyLightIntensity = 0.0f;
//	TArray<TObjectPtr<AActor>> LightStates;
//
//	int32 CurCornerX = -1;
//	int32 CurCornerY = -1;
//	float CurMinSlope = 0.0f;
//	FLinearColor CurMinSlopes = FLinearColor::White;
//	float LargestArea = 0.0f;
//	int32 CornerOffset = 0;
//	int32 CurrentCornerPass = 0;
//	TArray<FVector2D> PointArray;
//
//	TObjectPtr<UMaterialInterface> FullSphereMaterial;
//	TObjectPtr<UMaterialInterface> UpperHemisphereMaterial;
//	TObjectPtr<UMaterialInterface> BillboardMaterial;
//	float ConstantSpecular = 0.5f;
//	float ConstantRoughness = 0.5f;
//	float ConstantOpacity = 0.5f;
//	FLinearColor SubsurfaceColor = FLinearColor::White;
//	float ScatterMaskMin = 0.3f;
//	float ScatterMaskLength = 1.0f;
//	float MaskOffset = 0.0f;
//
//	bool DilationPasses = false;
//
//	TMap<EImpostorCaptureChannel, TObjectPtr<UTextureRenderTarget2D>> TargetChannelsMap;
//	TArray<TObjectPtr<UTextureRenderTarget2D>> SceneCaptureMipChain;
//	TObjectPtr<UTextureRenderTarget2D> CombinedAlphasRT;
//	TObjectPtr<UTextureRenderTarget2D> ScratchRT;
//};