#include "VertexColorProcessor.h"
#include "SHLSLCodeEditor.h"
#include "MaterialShaderType.h"
#include "MaterialShader.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "Shader.h"
#include "LevelEditor.h"
#include "ISinglePropertyView.h"
#include "SHLSLCodeEditor.h"
#include "Stats/Stats.h"
#include "RenderGraphEvent.h"
#include "RHIGPUReadback.h"
#include "ShaderFormatVectorVM.h"
#include "ShaderCompiler.h"
#include "Interfaces/IPluginManager.h"
#include "StaticMeshComponentLODInfo.h"

class FVertexColorProcesserShader : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FVertexColorProcesserShader);
	SHADER_USE_PARAMETER_STRUCT(FVertexColorProcesserShader, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, VertexCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<FVector3f>, PositionBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<FVector3f>, NormalBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<FVector2f>, UVBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<FVector4f>, ColorBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static inline void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}
};

IMPLEMENT_SHADER_TYPE(, FVertexColorProcesserShader, TEXT("/Plugins/ProceduralContentProcessor/VertexColorProcesserCS.usf"), TEXT("MainCS"), SF_Compute)
//IMPLEMENT_GLOBAL_SHADER(FVertexColorProcesserShader, "/Plugins/ProceduralContentProcessor/VertexColorProcesserCS.usf", "MainCS", SF_Compute);

UVertexColorProcessor::UVertexColorProcessor()
{
	DefaultCS = FText::FromString(R"(#include "/Engine/Public/Platform.ush"

uint VertexCount;
Buffer<float3> PositionBuffer;
Buffer<float3> NormalBuffer;
Buffer<float2> UVBuffer;

RWBuffer<float4> ColorBuffer;

float4 FillVertexColor(float3 Position, float3 Normal, float2 UV)
{
	return float4(1, 1, 1, 1);
}

[numthreads(128, 1, 1)]
void MainCS(
	uint3 DispatchThreadId : SV_DispatchThreadID,
	uint GroupIndex : SV_GroupIndex )
{
	const uint VertexIndex = DispatchThreadId.x;
	if (VertexIndex >= VertexCount)
		return;

    ColorBuffer[VertexIndex] = FillVertexColor(PositionBuffer[VertexIndex], NormalBuffer[VertexIndex], UVBuffer[VertexIndex]);
}
	)");
}

void UVertexColorProcessor::Activate()
{
	FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	OnActorSelectionChangedHandle = LevelEditor.OnActorSelectionChanged().AddUObject(this, &UVertexColorProcessor::OnActorSelectionChanged);
}

void UVertexColorProcessor::Deactivate()
{
	if (OnActorSelectionChangedHandle.IsValid()) {
		FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
		LevelEditor.OnActorSelectionChanged().Remove(OnActorSelectionChangedHandle);
		OnActorSelectionChangedHandle.Reset();
	}
}

TSharedPtr<SWidget> UVertexColorProcessor::BuildWidget()
{
	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				EditModule.CreateSingleProperty(this, "StaticMeshActor", FSinglePropertyParams()).ToSharedRef()
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew (SButton)
						.Text(FText::FromString("Reset"))
						.OnClicked_UObject(this, &UVertexColorProcessor::OnClickedReset)
					]
				+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SButton)
							.Text(FText::FromString("Execute"))
							.OnClicked_UObject(this, &UVertexColorProcessor::OnClickedExecute)
					]
			]
		]
		+ SVerticalBox::Slot()
		[
			SAssignNew(HLSLEditor, SHLSLCodeEditor, CurrentCode.IsEmpty()? DefaultCS: FText::FromString(CurrentCode))
		];
}

void UVertexColorProcessor::OnActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh)
{
	if (NewSelection.Num() > 0) {
		StaticMeshActor = Cast<AStaticMeshActor>(NewSelection[0]);
	}
}

FReply UVertexColorProcessor::OnClickedReset()
{
	if (HLSLEditor) {
		HLSLEditor->SetCode(DefaultCS);
	}
	return FReply::Handled();
}

FReply UVertexColorProcessor::OnClickedExecute()
{
	if (!StaticMeshActor) {
		return FReply::Handled();
	}
	CurrentCode = HLSLEditor->GetCode().ToString();
	FString FilePath = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("ProceduralContentProcessor"))->GetBaseDir(), TEXT("Shaders"), TEXT("VertexColorProcesserCS.usf"));
	bool Saved = FFileHelper::SaveStringToFile(CurrentCode, *FilePath);
	TryUpdateDefaultConfigFile();
	if (!Saved)
	{
		return FReply::Handled();
	}

	FlushShaderFileCache();

	FShaderCompilerInput Input;
	Input.Target = FShaderTarget(SF_Compute, SP_PCD3D_SM5);
	Input.VirtualSourceFilePath = TEXT("/Plugins/ProceduralContentProcessor/VertexColorProcesserCS.usf");
	Input.EntryPointName = TEXT("MainCS");
	Input.bSkipPreprocessedCache = false;
	FShaderCompilerOutput Output;
	FVectorVMCompilationOutput CompilationOutput;
	bool bSucceeded = CompileShader_VectorVM(Input, Output, FString(FPlatformProcess::ShaderDir()), 0, CompilationOutput, true);
	if (!bSucceeded)
	{
		UE_LOG(LogTemp, Error, TEXT("Test compile of %s took  seconds and failed.  Errors: "), *CompilationOutput.Errors);
		FFileHelper::SaveStringToFile(DefaultCS.ToString(), *FilePath);
		return FReply::Handled();
	}

	GEditor->Exec(GWorld ,TEXT("RecompileShaders Changed"), *GLog);
	GShaderCompilingManager->FinishAllCompilation();

	ENQUEUE_RENDER_COMMAND(ExecuteVertexColorProcessor)(
		[this](FRHICommandListImmediate& RHICmdList)
		{
			FRDGBuilder GraphBuilder(RHICmdList);
			TShaderMapRef<FVertexColorProcesserShader> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
			bool bIsShaderValid = ComputeShader.IsValid();
			if (bIsShaderValid) 
			{
				FVertexColorProcesserShader::FParameters* PassParameters = GraphBuilder.AllocParameters<FVertexColorProcesserShader::FParameters>();
				UStaticMeshComponent* StaticMeshComponent = StaticMeshActor->GetStaticMeshComponent();
				const FStaticMeshLODResources& LODModel = StaticMeshComponent->GetStaticMesh()->GetRenderData()->LODResources[0];
				
				TArray<FVector3f> PositionData;
				TArray<FVector3f> NormalData;
				TArray<FVector2f> UVData;
				PositionData.SetNum(LODModel.VertexBuffers.PositionVertexBuffer.GetNumVertices());
				NormalData.SetNum(LODModel.VertexBuffers.StaticMeshVertexBuffer.GetNumVertices());
				UVData.SetNum(LODModel.VertexBuffers.StaticMeshVertexBuffer.GetNumVertices());

				const FPositionVertex* PosPtr = static_cast<const FPositionVertex*>(LODModel.VertexBuffers.PositionVertexBuffer.GetVertexData());
				for (int32 i = 0; i < PositionData.Num(); ++i) 
				{
					PositionData[i] = PosPtr[i].Position;
				}

				if (LODModel.VertexBuffers.StaticMeshVertexBuffer.GetUseHighPrecisionTangentBasis())
				{
					typedef TStaticMeshVertexTangentDatum<typename TStaticMeshVertexTangentTypeSelector<EStaticMeshVertexTangentBasisType::HighPrecision>::TangentTypeT> TangentType;
					const TangentType* TangentPtr = static_cast<const TangentType*>(LODModel.VertexBuffers.StaticMeshVertexBuffer.GetTangentData());
					for (int32 i = 0; i < UVData.Num(); ++i) 
					{
						NormalData[i] = TangentPtr[i].GetTangentZ();
					}
				}
				else
				{
					typedef TStaticMeshVertexTangentDatum<typename TStaticMeshVertexTangentTypeSelector<EStaticMeshVertexTangentBasisType::Default>::TangentTypeT> TangentType;
					const TangentType* TangentPtr = static_cast<const TangentType*>(LODModel.VertexBuffers.StaticMeshVertexBuffer.GetTangentData());
					for (int32 i = 0; i < UVData.Num(); ++i) 
					{
						NormalData[i] = TangentPtr[i].GetTangentZ();
					}
				}

				const uint32 NumTexCoords = LODModel.VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords();
				if (LODModel.VertexBuffers.StaticMeshVertexBuffer.GetUseFullPrecisionUVs()) {
					typedef TStaticMeshVertexUVsDatum<typename TStaticMeshVertexUVsTypeSelector<EStaticMeshVertexUVType::HighPrecision>::UVsTypeT> UVType;
					const UVType* UVPtr = static_cast<const UVType*>(LODModel.VertexBuffers.StaticMeshVertexBuffer.GetTexCoordData());
					for (int32 i = 0; i < UVData.Num(); ++i) 
					{
						UVData[i] = UVPtr[i * NumTexCoords].GetUV();
					}

				}
				else {
					typedef TStaticMeshVertexUVsDatum<typename TStaticMeshVertexUVsTypeSelector<EStaticMeshVertexUVType::Default>::UVsTypeT> UVType;
					const UVType* UVPtr = static_cast<const UVType*>(LODModel.VertexBuffers.StaticMeshVertexBuffer.GetTexCoordData());
					for (int32 i = 0; i < UVData.Num(); ++i) 
					{
						UVData[i] = UVPtr[i * NumTexCoords].GetUV();
					}
				}

				FRDGBufferRef PositionBuffer = CreateUploadBuffer(GraphBuilder, TEXT("PositionBuffer"), 
					sizeof(FVector3f),
					PositionData.Num(),
					PositionData.GetData(),
					PositionData.Num() * sizeof(FVector3f)
				);

				FRDGBufferRef NormalBuffer = CreateUploadBuffer(GraphBuilder, TEXT("NormalBuffer"),
					sizeof(FVector3f),
					NormalData.Num(),
					NormalData.GetData(),
					NormalData.Num() * sizeof(FVector3f)
				);

				FRDGBufferRef UVBuffer = CreateUploadBuffer(GraphBuilder, TEXT("UVBuffer"),
					sizeof(FVector2f),
					UVData.Num(),
					UVData.GetData(),
					UVData.Num() * sizeof(FVector2f)
				);
	
				FRDGBufferRef ColorBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(FVector4f), LODModel.GetNumVertices()), TEXT("ColorBuffer"));
				PassParameters->VertexCount = LODModel.GetNumVertices();
				PassParameters->PositionBuffer = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(PositionBuffer, PF_R32G32B32_UINT));
				PassParameters->NormalBuffer = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(NormalBuffer, PF_R32G32B32_UINT));
				PassParameters->UVBuffer = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(PositionBuffer, PF_R32G32_UINT));
				PassParameters->ColorBuffer = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(ColorBuffer, PF_R32G32B32A32_UINT));

				auto GroupCount = FComputeShaderUtils::GetGroupCount(LODModel.GetNumVertices(), 128);
				GraphBuilder.AddPass(
					RDG_EVENT_NAME("ExecuteVertexColorProcesser"),
					PassParameters,
					ERDGPassFlags::AsyncCompute,
					[&PassParameters, ComputeShader, GroupCount](FRHIComputeCommandList& RHICmdList)
					{ 
						FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, GroupCount);
					});

				FRHIGPUBufferReadback* GPUBufferReadback = new FRHIGPUBufferReadback(TEXT("ExecuteVertexColorProcessorOutput"));

				uint32 ResultSize = LODModel.GetNumVertices() * sizeof(FVector4f);
				AddEnqueueCopyPass(GraphBuilder, GPUBufferReadback, ColorBuffer, ResultSize);
				auto RunnerFunc = [GPUBufferReadback, this, ResultSize](auto&& RunnerFunc) -> void {
					if (GPUBufferReadback->IsReady()) {
						FVector4f* Buffer = (FVector4f*)GPUBufferReadback->Lock(ResultSize);
						TArray<FVector4f> Result(Buffer, ResultSize / sizeof(FVector4f));
						GPUBufferReadback->Unlock();
						AsyncTask(ENamedThreads::GameThread, [Result, this]() {
							OnApplyVertexColor(Result);
						});
						delete GPUBufferReadback;
					}
					else {
						AsyncTask(ENamedThreads::ActualRenderingThread, [RunnerFunc]() {
							RunnerFunc(RunnerFunc);
						});
					}
				};
				AsyncTask(ENamedThreads::ActualRenderingThread, [RunnerFunc]() {
					RunnerFunc(RunnerFunc);
				});
			}
			GraphBuilder.Execute();
		});
	return FReply::Handled();
}

void UVertexColorProcessor::OnApplyVertexColor(TArray<FVector4f> VertexColors)
{
	for (int32 i = 0; i < VertexColors.Num(); ++i) {
		UE_LOG(LogTemp, Warning, TEXT("VertexColor %d: %s"), i, *VertexColors[i].ToString());
	}
	if (!StaticMeshActor) {
		return;
	}
	UStaticMeshComponent* StaticMeshComponent = StaticMeshActor->GetStaticMeshComponent();

	// If a static mesh component was found, apply LOD0 painting to all lower LODs.
	if (!StaticMeshComponent || !StaticMeshComponent->GetStaticMesh())
	{
		return;
	}

	if (StaticMeshComponent->LODData.Num() < 1)
	{
		//We need at least some painting on the base LOD to apply it to the lower LODs
		return;
	}

	StaticMeshComponent->bCustomOverrideVertexColorPerLOD = false;

	uint32 NumLODs = StaticMeshComponent->GetStaticMesh()->GetRenderData()->LODResources.Num();
	StaticMeshComponent->Modify();

	// Ensure LODData has enough entries in it, free not required.
	StaticMeshComponent->SetLODDataCount(NumLODs, StaticMeshComponent->LODData.Num());

	const FStaticMeshLODResources& LODModel = StaticMeshComponent->GetStaticMesh()->GetRenderData()->LODResources[0];
	FStaticMeshComponentLODInfo& ComponentLodInfo = StaticMeshComponent->LODData[0];
	if (ComponentLodInfo.OverrideVertexColors)
	{
		const int32 NumVertices = ComponentLodInfo.OverrideVertexColors->GetNumVertices();
		for (int32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
		{
			if (VertexIndex < VertexColors.Num()) {
				ComponentLodInfo.OverrideVertexColors->VertexColor(VertexIndex) = FLinearColor(VertexColors[VertexIndex]).ToFColor(false);
			}
		}
	}
	else
	{
		// Initialize vertex buffer from given color
		ComponentLodInfo.OverrideVertexColors = new FColorVertexBuffer;
		FColor NewFillColor(EForceInit::ForceInitToZero);
		ComponentLodInfo.OverrideVertexColors->InitFromSingleColor(FColor::Blue, LODModel.GetNumVertices());
	}
	
	ComponentLodInfo.PaintedVertices.Empty();
	StaticMeshComponent->CachePaintedDataIfNecessary();
	BeginInitResource(ComponentLodInfo.OverrideVertexColors);

	for (uint32 i = 1; i < NumLODs; ++i)
	{
		FStaticMeshComponentLODInfo* CurrInstanceMeshLODInfo = &StaticMeshComponent->LODData[i];
		FStaticMeshLODResources& CurrRenderData = StaticMeshComponent->GetStaticMesh()->GetRenderData()->LODResources[i];
		// Destroy the instance vertex  color array if it doesn't fit
		if (CurrInstanceMeshLODInfo->OverrideVertexColors
			&& CurrInstanceMeshLODInfo->OverrideVertexColors->GetNumVertices() != CurrRenderData.GetNumVertices())
		{
			CurrInstanceMeshLODInfo->ReleaseOverrideVertexColorsAndBlock();
		}

		if (CurrInstanceMeshLODInfo->OverrideVertexColors)
		{
			CurrInstanceMeshLODInfo->BeginReleaseOverrideVertexColors();
		}
		else
		{
			// Setup the instance vertex color array if we don't have one yet
			CurrInstanceMeshLODInfo->OverrideVertexColors = new FColorVertexBuffer;
		}
	}

	FlushRenderingCommands();

	const FStaticMeshComponentLODInfo& SourceCompLODInfo = StaticMeshComponent->LODData[0];
	const FStaticMeshLODResources& SourceRenderData = StaticMeshComponent->GetStaticMesh()->GetRenderData()->LODResources[0];
	for (uint32 i = 1; i < NumLODs; ++i)
	{
		FStaticMeshComponentLODInfo& CurCompLODInfo = StaticMeshComponent->LODData[i];
		FStaticMeshLODResources& CurRenderData = StaticMeshComponent->GetStaticMesh()->GetRenderData()->LODResources[i];

		check(CurCompLODInfo.OverrideVertexColors);
		check(SourceCompLODInfo.OverrideVertexColors);

		TArray<FColor> NewOverrideColors;

		RemapPaintedVertexColors(
			SourceCompLODInfo.PaintedVertices,
			SourceCompLODInfo.OverrideVertexColors,
			SourceRenderData.VertexBuffers.PositionVertexBuffer,
			SourceRenderData.VertexBuffers.StaticMeshVertexBuffer,
			CurRenderData.VertexBuffers.PositionVertexBuffer,
			&CurRenderData.VertexBuffers.StaticMeshVertexBuffer,
			NewOverrideColors
		);

		if (NewOverrideColors.Num())
		{
			CurCompLODInfo.OverrideVertexColors->InitFromColorArray(NewOverrideColors);
		}

		// Initialize the vert. colors
		BeginInitResource(CurCompLODInfo.OverrideVertexColors);
	}
}