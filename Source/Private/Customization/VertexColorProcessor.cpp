#include "VertexColorProcessor.h"
#include "SHLSLCodeEditor.h"
#include "MaterialShaderType.h"
#include "MaterialShader.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "Shader.h"
#include "LevelEditor.h"
#include "Engine/StaticMeshActor.h"
#include "ISinglePropertyView.h"
#include "SHLSLCodeEditor.h"
#include "Stats/Stats.h"
#include "RenderGraphEvent.h"
#include "RHIGPUReadback.h"
#include "ShaderFormatVectorVM.h"
#include "ShaderCompiler.h"
#include "Interfaces/IPluginManager.h"

#define NUM_THREADS_PER_GROUP_DIMENSION 8

class FVertexColorProcesserShader : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FVertexColorProcesserShader);
	SHADER_USE_PARAMETER_STRUCT(FVertexColorProcesserShader, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<int>, Input)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<int>, Output)
	END_SHADER_PARAMETER_STRUCT()

	static inline void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADS_X"), 1);
		OutEnvironment.SetDefine(TEXT("THREADS_Y"), 1);
		OutEnvironment.SetDefine(TEXT("THREADS_Z"), 1);
	}
};

IMPLEMENT_SHADER_TYPE(, FVertexColorProcesserShader, TEXT("/Plugins/ProceduralContentProcessor/VertexColorProcesserCS.usf"), TEXT("MainCS"), SF_Compute)
//IMPLEMENT_GLOBAL_SHADER(FVertexColorProcesserShader, "/Plugins/ProceduralContentProcessor/VertexColorProcesserCS.usf", "MainCS", SF_Compute);

UVertexColorProcessor::UVertexColorProcessor()
{
	DefaultCS = FText::FromString(R"(#include "/Engine/Public/Platform.ush"

Buffer<int> Input;
RWBuffer<int> Output;
[numthreads(THREADS_X, THREADS_Y, THREADS_Z)]
void MainCS(
	uint3 DispatchThreadId : SV_DispatchThreadID,
	uint GroupIndex : SV_GroupIndex )
{
	// Outputs one number
	Output[0] = Input[0] * Input[1];
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
				EditModule.CreateSingleProperty(this, "StaticMesh", FSinglePropertyParams()).ToSharedRef()
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
			SAssignNew(HLSLEditor, SHLSLCodeEditor, DefaultCS)
		];
}

void UVertexColorProcessor::OnActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh)
{
	if (NewSelection.Num() > 0) {
		if (auto MeshActor = Cast<AStaticMeshActor>(NewSelection[0])) {
			StaticMesh = MeshActor->GetStaticMeshComponent()->GetStaticMesh();
		}
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
	FString FilePath = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("ProceduralContentProcessor"))->GetBaseDir(), TEXT("Shaders"), TEXT("VertexColorProcesserCS.usf"));
	FFileHelper::SaveStringToFile(HLSLEditor->GetCode().ToString(), *FilePath);
	FlushShaderFileCache();

	FShaderCompilerInput Input;
	Input.Target = FShaderTarget(SF_Compute, SP_PCD3D_SM5);
	Input.VirtualSourceFilePath = TEXT("/Plugins/ProceduralContentProcessor/VertexColorProcesserCS.usf");
	Input.EntryPointName = TEXT("MainCS");
	Input.Environment.SetDefine(TEXT("THREADS_X"), 1);
	Input.Environment.SetDefine(TEXT("THREADS_Y"), 1);
	Input.Environment.SetDefine(TEXT("THREADS_Z"), 1);
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
			if (bIsShaderValid) {
				FVertexColorProcesserShader::FParameters* PassParameters = GraphBuilder.AllocParameters<FVertexColorProcesserShader::FParameters>();
				int Input[2] = {1 ,2 };
				const void* RawData = (void*)Input;
				int NumInputs = 2;
				int InputSize = sizeof(int);
				FRDGBufferRef InputBuffer = CreateUploadBuffer(GraphBuilder, TEXT("InputBuffer"), InputSize, NumInputs, RawData, InputSize * NumInputs);
				PassParameters->Input = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(InputBuffer, PF_R32_SINT));
				FRDGBufferRef OutputBuffer = GraphBuilder.CreateBuffer(
					FRDGBufferDesc::CreateBufferDesc(sizeof(int32), 1),
					TEXT("OutputBuffer"));
				PassParameters->Output = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(OutputBuffer, PF_R32_SINT));
				auto GroupCount = FComputeShaderUtils::GetGroupCount(FIntVector(1, 1, 1), FComputeShaderUtils::kGolden2DGroupSize);
				GraphBuilder.AddPass(
					RDG_EVENT_NAME("ExecuteMySimpleComputeShader"),
					PassParameters,
					ERDGPassFlags::AsyncCompute,
					[&PassParameters, ComputeShader, GroupCount](FRHIComputeCommandList& RHICmdList)
					{ 
						FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, GroupCount);
					});

				FRHIGPUBufferReadback* GPUBufferReadback = new FRHIGPUBufferReadback(TEXT("ExecuteVertexColorProcessorOutput"));
				AddEnqueueCopyPass(GraphBuilder, GPUBufferReadback, OutputBuffer, 0u);
				auto RunnerFunc = [GPUBufferReadback](auto&& RunnerFunc) -> void {
					if (GPUBufferReadback->IsReady()) {

						int32* Buffer = (int32*)GPUBufferReadback->Lock(1);
						int OutVal = Buffer[0];

						GPUBufferReadback->Unlock();

						AsyncTask(ENamedThreads::GameThread, [OutVal]() {
							UE_LOG(LogTemp, Warning,TEXT("!!!!!!!! Readback: %d"), OutVal);
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

