#include "ProceduralContentProcessorLibrary.h"
#include "Engine/StaticMeshActor.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialExpressionTime.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "Misc/ScopedSlowTask.h"
#include "LandscapeStreamingProxy.h"
#include <Kismet/GameplayStatics.h>
#include "Selection.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "WorldPartition/HLOD/HLODLayer.h"
#include "InstancedFoliageActor.h"
#include "Components/LightComponentBase.h"
#include "Layers/LayersSubsystem.h"
#include "DataLayer/DataLayerEditorSubsystem.h"
#include "EditPivotTool.h"
#include "ToolTargets/StaticMeshComponentToolTarget.h"
#include "Engine/StaticMeshActor.h"
#include "InteractiveToolManager.h"
#include "InteractiveToolsContext.h"
#include "BaseGizmos/TransformGizmoUtil.h"
#include "EdModeInteractiveToolsContext.h"
#include "EditorModeManager.h"
#include "ToolTargetManager.h"
#include "Blueprint/UserWidget.h"

#define LOCTEXT_NAMESPACE "ProceduralContentProcessor"

void UProceduralContentProcessorLibrary::ClearPropertyMaterix(FProceduralPropertyMatrix& Matrix)
{
	Matrix.ObjectInfoList.Reset();
	Matrix.ObjectInfoMap.Reset();
	Matrix.bIsDirty = true;
}

void UProceduralContentProcessorLibrary::AddPropertyMaterixField(FProceduralPropertyMatrix& Matrix, UObject* InObject, FName InPropertyName)
{
	Matrix.FieldKeys.AddUnique(InPropertyName);
	auto InfoPtr = Matrix.ObjectInfoMap.Find(InObject);
	TSharedPtr<FProceduralPropertyMatrixInfo> Info;
	if (!InfoPtr) {
		Info = MakeShared<FProceduralPropertyMatrixInfo>();
		Info->Object = InObject;
		Matrix.ObjectInfoMap.Add(InObject, Info);
		Matrix.ObjectInfoList.Add(Info);
	}
	else {
		Info = *InfoPtr;
	}
	Info->Fields.AddUnique({ InPropertyName ,FString()});
	Matrix.bIsDirty = true;
}

void UProceduralContentProcessorLibrary::AddPropertyMaterixFieldValue(FProceduralPropertyMatrix& Matrix, UObject* InObject, FName InFieldName, FString InFieldValue)
{
	Matrix.FieldKeys.AddUnique(InFieldName);
	auto InfoPtr = Matrix.ObjectInfoMap.Find(InObject);
	TSharedPtr<FProceduralPropertyMatrixInfo> Info;
	if (!InfoPtr) {
		Info = MakeShared<FProceduralPropertyMatrixInfo>();
		Info->Object = InObject;
		Matrix.ObjectInfoMap.Add(InObject, Info);
		Matrix.ObjectInfoList.Add(Info);
	}
	else {
		Info = *InfoPtr;
	}
	Info->Fields.AddUnique({ InFieldName ,InFieldValue });
	Matrix.bIsDirty = true;
}

void UProceduralContentProcessorLibrary::BeginTransaction(FText Text)
{
	GEditor->BeginTransaction(Text);
}

void UProceduralContentProcessorLibrary::ModifyObject(UObject* Object)
{
	if (!Object->HasAnyFlags(RF_Transactional))
		Object->SetFlags(RF_Transactional);
	Object->Modify();
}

void UProceduralContentProcessorLibrary::EndTransaction()
{
	GEditor->EndTransaction();
}

UObject* UProceduralContentProcessorLibrary::DuplicateObject(UObject* SourceObject, UObject* Outer)
{
	return ::DuplicateObject<UObject>(SourceObject, Outer == nullptr ? GetTransientPackage() : Outer);
}

UUserWidget* UProceduralContentProcessorLibrary::PushDialog(UObject* Outer, TSubclassOf<UUserWidget> WidgetClass, FText WindowTitle /*= INVTEXT("Dialog")*/, FVector2D DialogSize /*= FVector2D(400, 300)*/, bool IsModalWindow /*= true*/)
{
	UUserWidget* UserWidget = ::NewObject<UUserWidget>(Outer, WidgetClass.Get());
	auto NewWindw = SNew(SWindow)
		.Title(WindowTitle)
		.ClientSize(DialogSize);
	if (IsModalWindow) {
		NewWindw->SetAsModalWindow();
	}
	TSharedPtr<SWindow> ParentWindow;
	if (auto Widget = Cast<UUserWidget>(Outer)) {
		if (Widget->GetCachedWidget()) {
			ParentWindow = FSlateApplication::Get().FindWidgetWindow(Widget->GetCachedWidget().ToSharedRef());
		}
	}
	if (ParentWindow) {
		FSlateApplication::Get().AddWindowAsNativeChild(NewWindw, ParentWindow.ToSharedRef())->SetContent
		(
			UserWidget->TakeWidget()
		);
	}
	else {
		FSlateApplication::Get().AddWindow(NewWindw)->SetContent
		(
			UserWidget->TakeWidget()
		);
	}
	return UserWidget;
}

EMsgBoxReturnType UProceduralContentProcessorLibrary::PushMsgBox(EMsgBoxType Type, FText Msg)
{
	EAppReturnType::Type Ret = FMessageDialog::Open((EAppMsgType::Type)Type, Msg);
	return EMsgBoxReturnType(Ret);
}

UUserWidget* UProceduralContentProcessorLibrary::PushContextMenu(UUserWidget* InParentWidget, TSubclassOf<UUserWidget> WidgetClass, FVector2D SummonLocation /*= FVector2D::ZeroVector*/)
{
	if (!WidgetClass)
		return nullptr;
	UUserWidget* UserWidget = ::NewObject<UUserWidget>(InParentWidget, WidgetClass.Get());
	if (SummonLocation == FVector2D::ZeroVector)
		SummonLocation = FSlateApplication::Get().GetCursorPos();
	TSharedPtr<SWidget> ParentWidget = InParentWidget->GetCachedWidget();
	if (ParentWidget.IsValid()) {
		FSlateApplication::Get().PushMenu(
			ParentWidget.ToSharedRef(),
			FWidgetPath(),
			UserWidget->TakeWidget(),
			SummonLocation,
			FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("PushContextMenu: Parent widget is invaild!"));
	}
	return UserWidget;
}

void UProceduralContentProcessorLibrary::DismissAllMenus()
{
	FSlateApplication::Get().DismissAllMenus();
}

bool UProceduralContentProcessorLibrary::IsNaniteEnable(UStaticMesh* InMesh)
{
	return InMesh ? (bool )InMesh->NaniteSettings.bEnabled: false;
}

void UProceduralContentProcessorLibrary::GenerateStaticMeshLODs(UStaticMesh* InMesh, FName LODGroup, int LODCount)
{
	if (LODGroup != InMesh->LODGroup ||
		(LODGroup == InMesh->LODGroup && InMesh->GetNumSourceModels() != LODCount)) {
		InMesh->Modify();
		InMesh->SetLODGroup(LODGroup);
		InMesh->SetNumSourceModels(LODCount);
		InMesh->GenerateLodsInPackage();
	}
}

bool UProceduralContentProcessorLibrary::IsDynamicMaterial(AActor* InActor)
{
	if (AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>(InActor)) {
		if (StaticMeshActor && StaticMeshActor->GetStaticMeshComponent()) {
			TArray<UMaterialInterface*> Materials = StaticMeshActor->GetStaticMeshComponent()->GetMaterials();
			TQueue<UMaterialInterface*> Queue;
			for (auto Material : Materials) {
				Queue.Enqueue(Material);
			}
			while (!Queue.IsEmpty()) {
				UMaterialInterface* Top;
				Queue.Dequeue(Top);
				if (auto MatIns = Cast<UMaterialInstance>(Top)) {
					Queue.Enqueue(MatIns->Parent);
				}
				else if (auto Mat = Cast<UMaterial>(Top)) {
					UObjectBase* TimeNode = FindObjectWithOuter(Mat, UMaterialExpressionTime::StaticClass());
					if (TimeNode) {
						InActor->SetIsTemporarilyHiddenInEditor(true);
						return true;
					}
				}
			}
		}
	}
	return false;
}

bool UProceduralContentProcessorLibrary::MatchString(FString InString, const TArray<FString>& IncludeList, const TArray<FString>& ExcludeList)
{
	for (const auto& Inc : IncludeList) {
		if (InString.Contains(Inc) || Inc.IsEmpty()) {
			for (const auto& Exc : ExcludeList) {
				if (InString.Contains(Exc)) {
					return false;
				}
			}
			return true;
		}
	}
	return false;
}

void UProceduralContentProcessorLibrary::SetNaniteMeshEnabled(UStaticMesh* StaticMesh, bool bEnabled)
{
	StaticMesh->Modify();
	StaticMesh->NaniteSettings.bEnabled = bEnabled;
	FProperty* ChangedProperty = FindFProperty<FProperty>(UStaticMesh::StaticClass(), GET_MEMBER_NAME_CHECKED(UStaticMesh, NaniteSettings));
	FPropertyChangedEvent Event(ChangedProperty);
	StaticMesh->PostEditChangeProperty(Event);
}

bool UProceduralContentProcessorLibrary::IsGeneratedByBlueprint(UObject* InObject)
{
	return InObject&& InObject->GetClass()&&Cast<UBlueprint>(InObject->GetClass()->ClassGeneratedBy);
}

UObject* UProceduralContentProcessorLibrary::CopyProperties(UObject* OldObject, UObject* NewObject)
{
	UEngine::FCopyPropertiesForUnrelatedObjectsParams Params;
	Params.bNotifyObjectReplacement = true;
	UEngine::CopyPropertiesForUnrelatedObjects(OldObject, NewObject);
	return NewObject;
}

UStaticMesh* UProceduralContentProcessorLibrary::GetComplexCollisionMesh(UStaticMesh* InMesh)
{
	if (!InMesh)
		return nullptr;
	return InMesh->ComplexCollisionMesh;
}

void UProceduralContentProcessorLibrary::SetStaticMeshPivot(UStaticMesh* InStaticMesh, EStaticMeshPivotType PivotType)
{
	if(InStaticMesh == nullptr)
		return;
	auto InteractiveToolsContext = NewObject<UEditorInteractiveToolsContext>();
	InteractiveToolsContext->InitializeContextWithEditorModeManager(&GLevelEditorModeTools(), nullptr);
	UE::TransformGizmoUtil::RegisterTransformGizmoContextObject(InteractiveToolsContext);

	InteractiveToolsContext->ToolManager->RegisterToolType("EditPivotTool", NewObject<UEditPivotToolBuilder>());
	UStaticMeshComponent* StaticMeshComp = NewObject<UStaticMeshComponent>();
	StaticMeshComp->SetStaticMesh(InStaticMesh);

	InteractiveToolsContext->TargetManager->AddTargetFactory(NewObject<UStaticMeshComponentToolTargetFactory>());
	UToolTarget* Target = InteractiveToolsContext->TargetManager->BuildTarget(StaticMeshComp, FToolTargetTypeRequirements());

	InteractiveToolsContext->ToolManager->SelectActiveToolType(EToolSide::Left, "EditPivotTool");
	GLevelEditorModeTools().SelectNone();
	GLevelEditorModeTools().GetSelectedComponents()->Select(StaticMeshComp);

	InteractiveToolsContext->ToolManager->ActivateTool(EToolSide::Left);
	if (auto EditTool = Cast<UEditPivotTool>(InteractiveToolsContext->ToolManager->ActiveLeftTool))
	{
		EditTool->SetTargets({ Target });
		EditTool->RequestAction((EEditPivotToolActions)PivotType);
		EditTool->Tick(0.1);
		EditTool->Shutdown(EToolShutdownType::Accept);
	}
}

#undef LOCTEXT_NAMESPACE