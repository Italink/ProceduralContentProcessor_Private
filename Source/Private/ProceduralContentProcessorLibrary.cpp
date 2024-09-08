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
#include "ObjectTools.h"
#include "ReferencedAssetsUtils.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "StaticMeshEditorSubsystem.h"

#define LOCTEXT_NAMESPACE "ProceduralContentProcessor"

void UProceduralContentProcessorLibrary::ClearObjectMaterix(FProceduralObjectMatrix& Matrix)
{
	Matrix.ObjectInfoList.Reset();
	Matrix.ObjectInfoMap.Reset();
	Matrix.bIsDirty = true;
}

void UProceduralContentProcessorLibrary::AddPropertyFieldWithOwner(FProceduralObjectMatrix& Matrix, UObject* InOwner, UObject* InObject, FName InPropertyName)
{
	Matrix.FieldKeys.AddUnique(InPropertyName);
	auto InfoPtr = Matrix.ObjectInfoMap.Find(InOwner);
	TSharedPtr<FProceduralObjectMatrixRow> Info;
	if (!InfoPtr) {
		Info = MakeShared<FProceduralObjectMatrixRow>();
		Info->Owner = InOwner;
		Matrix.ObjectInfoMap.Add(InOwner, Info);
		Matrix.ObjectInfoList.Add(Info);
	}
	else {
		Info = *InfoPtr;
	}

	TSharedPtr<FProceduralObjectMatrixPropertyField> Field = MakeShared<FProceduralObjectMatrixPropertyField>();
	Field->Owner = InOwner;
	Field->Object = InObject;
	Field->Name = InPropertyName;
	Field->PropertyName = InPropertyName.ToString();
	Info->AddField(Field);

	Matrix.bIsDirty = true;
}

void UProceduralContentProcessorLibrary::AddPropertyField(FProceduralObjectMatrix& Matrix, UObject* InObject, FName InPropertyName)
{
	Matrix.FieldKeys.AddUnique(InPropertyName);
	auto InfoPtr = Matrix.ObjectInfoMap.Find(InObject);
	TSharedPtr<FProceduralObjectMatrixRow> Info;
	if (!InfoPtr) {
		Info = MakeShared<FProceduralObjectMatrixRow>();
		Info->Owner = InObject;
		Matrix.ObjectInfoMap.Add(InObject, Info);
		Matrix.ObjectInfoList.Add(Info);
	}
	else {
		Info = *InfoPtr;
	}

	TSharedPtr<FProceduralObjectMatrixPropertyField> Field = MakeShared<FProceduralObjectMatrixPropertyField>();
	Field->Owner = InObject;
	Field->Object = InObject;
	Field->Name = InPropertyName;
	Field->PropertyName = InPropertyName.ToString();
	Info->AddField(Field);

	Matrix.bIsDirty = true;
}

void UProceduralContentProcessorLibrary::AddTextField(FProceduralObjectMatrix& Matrix, UObject* InObject, FName InFieldName, FString InFieldValue)
{
	Matrix.FieldKeys.AddUnique(InFieldName);
	auto InfoPtr = Matrix.ObjectInfoMap.Find(InObject);
	TSharedPtr<FProceduralObjectMatrixRow> Info;
	if (!InfoPtr) {
		Info = MakeShared<FProceduralObjectMatrixRow>();
		Info->Owner = InObject;
		Matrix.ObjectInfoMap.Add(InObject, Info);
		Matrix.ObjectInfoList.Add(Info);;
	}
	else {
		Info = *InfoPtr;
	}
	TSharedPtr<FProceduralObjectMatrixTextField> Field = MakeShared<FProceduralObjectMatrixTextField>();
	Field->Owner = InObject;
	Field->Name = InFieldName;
	Field->Text = InFieldValue;
	Info->AddField(Field);
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

TArray<UClass*> UProceduralContentProcessorLibrary::GetDerivedClasses(const UClass* ClassToLookFor, bool bRecursive)
{
	TArray<UClass*> Results;
	::GetDerivedClasses(ClassToLookFor, Results, bRecursive);
	return Results;
}

UObject* UProceduralContentProcessorLibrary::DuplicateObject(UObject* SourceObject, UObject* Outer)
{
	return ::DuplicateObject<UObject>(SourceObject, Outer == nullptr ? GetTransientPackage() : Outer);
}

int32 UProceduralContentProcessorLibrary::DeleteObjects(const TArray< UObject* >& ObjectsToDelete, bool bShowConfirmation /*= true*/, bool bAllowCancelDuringDelete /*= true*/)
{
	return ObjectTools::DeleteObjects(ObjectsToDelete, bShowConfirmation, bAllowCancelDuringDelete ? ObjectTools::EAllowCancelDuringDelete::AllowCancel : ObjectTools::EAllowCancelDuringDelete::CancelNotAllowed);
}

int32 UProceduralContentProcessorLibrary::DeleteObjectsUnchecked(const TArray< UObject* >& ObjectsToDelete)
{
	return ObjectTools::DeleteObjectsUnchecked(ObjectsToDelete);
}

void UProceduralContentProcessorLibrary::ConsolidateObjects(UObject* ObjectToConsolidateTo, TArray<UObject*>& ObjectsToConsolidate, bool bShowDeleteConfirmation /*= true*/)
{
	ObjectTools::ConsolidateObjects(ObjectToConsolidateTo, ObjectsToConsolidate, bShowDeleteConfirmation);
}

TSet<UObject*> UProceduralContentProcessorLibrary::GetAssetReferences(UObject* Object, const TArray<UClass*>& IgnoreClasses, bool bIncludeDefaultRefs /*= false*/)
{
	TSet<UObject*> ReferencedAssets;
	FFindReferencedAssets::BuildAssetList(Object, IgnoreClasses, {}, ReferencedAssets, bIncludeDefaultRefs);
	return ReferencedAssets;
}

UUserWidget* UProceduralContentProcessorLibrary::AddDialog(UObject* Outer, TSubclassOf<UUserWidget> WidgetClass, FText WindowTitle /*= INVTEXT("Dialog")*/, FVector2D DialogSize /*= FVector2D(400, 300)*/, bool IsModalWindow /*= true*/)
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

EMsgBoxReturnType UProceduralContentProcessorLibrary::AddMessageBox(EMsgBoxType Type, FText Msg)
{
	EAppReturnType::Type Ret = FMessageDialog::Open((EAppMsgType::Type)Type, Msg);
	return EMsgBoxReturnType(Ret);
}

UUserWidget* UProceduralContentProcessorLibrary::AddContextMenu(UUserWidget* InParentWidget, TSubclassOf<UUserWidget> WidgetClass, FVector2D SummonLocation /*= FVector2D::ZeroVector*/)
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

void UProceduralContentProcessorLibrary::AddNotification(FText Notification, float FadeInDuration /*= 2*/, float ExpireDuration /*= 2*/, float FadeOutDuration /*= 2*/)
{
	FNotificationInfo Info(Notification);
	Info.FadeInDuration = FadeInDuration;
	Info.ExpireDuration = ExpireDuration;
	Info.FadeOutDuration = FadeOutDuration;
	FSlateNotificationManager::Get().AddNotification(Info);
}

void UProceduralContentProcessorLibrary::AddNotificationWithButton(FText Notification, UObject* Object, TArray<FName> FunctionNames)
{
	if (Object == nullptr)
		return;
	static TWeakPtr<SNotificationItem> NotificationPtr;
	FNotificationInfo Info(Notification);
	Info.FadeInDuration = 2.0f;
	Info.FadeOutDuration = 2.0f;
	Info.bFireAndForget = false;
	Info.bUseThrobber = false;
	for (auto FunctionName : FunctionNames) {
		FNotificationButtonInfo Button = FNotificationButtonInfo(
			FText::FromName(FunctionName),
			FText::FromName(FunctionName),
			FSimpleDelegate::CreateLambda([Object, FunctionName]() {
				TSharedPtr<SNotificationItem> Notification = NotificationPtr.Pin();
				if (Notification.IsValid()){
					if (UFunction* Function = Object->GetClass()->FindFunctionByName(FunctionName)) {
						Object->ProcessEvent(Function, nullptr);
					}
					Notification->SetEnabled(false);
					Notification->SetExpireDuration(0.0f);
					Notification->ExpireAndFadeout();
					NotificationPtr.Reset();
				}
				}),
			SNotificationItem::ECompletionState::CS_None
		);
		Info.ButtonDetails.Add(Button);
	}
	NotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
}

void UProceduralContentProcessorLibrary::PushSlowTask(FText TaskName, float AmountOfWork)
{
	TSharedPtr<FSlowTask> SlowTask = MakeShared<FSlowTask>(AmountOfWork, TaskName);
	SlowTasks.Add(SlowTask);
	SlowTask->Initialize();
	SlowTask->MakeDialog();
}

void UProceduralContentProcessorLibrary::EnterSlowTaskProgressFrame(float ExpectedWorkThisFrame /*= 1.f*/, const FText& Text /*= FText()*/)
{
	if (!SlowTasks.IsEmpty()) {
		SlowTasks[SlowTasks.Num() - 1]->EnterProgressFrame(ExpectedWorkThisFrame, Text);
	}
}

void UProceduralContentProcessorLibrary::PopSlowTask()
{
	if (!SlowTasks.IsEmpty()) {
		SlowTasks[SlowTasks.Num() - 1]->Destroy();
		SlowTasks.SetNum(SlowTasks.Num() - 1);
	}
}

void UProceduralContentProcessorLibrary::ClearAllSlowTask()
{
	for (auto Task : SlowTasks) {
		Task->Destroy();
	}
	SlowTasks.Reset();
}

bool UProceduralContentProcessorLibrary::IsNaniteEnable(UStaticMesh* InMesh)
{
	return InMesh ? (bool )InMesh->NaniteSettings.bEnabled: false;
}

void UProceduralContentProcessorLibrary::SetNaniteMeshEnabled(UStaticMesh* StaticMesh, bool bEnabled)
{
	StaticMesh->Modify();
	StaticMesh->NaniteSettings.bEnabled = bEnabled;
	FProperty* ChangedProperty = FindFProperty<FProperty>(UStaticMesh::StaticClass(), GET_MEMBER_NAME_CHECKED(UStaticMesh, NaniteSettings));
	FPropertyChangedEvent Event(ChangedProperty);
	StaticMesh->PostEditChangeProperty(Event);
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

bool UProceduralContentProcessorLibrary::IsMaterialHasTimeNode(AStaticMeshActor* StaticMeshActor)
{
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
					StaticMeshActor->SetIsTemporarilyHiddenInEditor(true);
					return true;
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

UStaticMeshEditorSubsystem* UProceduralContentProcessorLibrary::GetStaticMeshEditorSubsystem()
{
	return GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();
}

TArray<FStaticMeshLODInfo> UProceduralContentProcessorLibrary::GetStaticMeshLODInfos(UStaticMesh* InStaticMesh, bool bUseDistance /*= false*/, bool bEnableBuildSetting /*= false*/)
{
	TArray<FStaticMeshLODInfo> Infos;
	if (InStaticMesh == nullptr)
		return Infos;
	Infos.SetNum(InStaticMesh->GetNumSourceModels());
	for (int i = 0; i < Infos.Num(); i++) {
		Infos[i].bEnableBuildSetting = bEnableBuildSetting;
		Infos[i].bUseDistance = bUseDistance;
		Infos[i].BuildSettings = InStaticMesh->GetSourceModel(i).BuildSettings;
		Infos[i].ReductionSettings = InStaticMesh->GetSourceModel(i).ReductionSettings;
		Infos[i].ScreenSize = InStaticMesh->GetSourceModel(i).ScreenSize.GetValue();
		Infos[i].Distance = GetLodDistance(InStaticMesh, i);
	}
	return Infos;
}

void UProceduralContentProcessorLibrary::SetStaticMeshLODInfos(UStaticMesh* InStaticMesh, TArray<FStaticMeshLODInfo> InInfos)
{
	if (InStaticMesh == nullptr || InStaticMesh->GetNumSourceModels() == 0 || InInfos.IsEmpty())
		return ;
	bool bStaticMeshIsEdited = false;
	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (AssetEditorSubsystem->FindEditorForAsset(InStaticMesh, false))
	{
		AssetEditorSubsystem->CloseAllEditorsForAsset(InStaticMesh);
		bStaticMeshIsEdited = true;
	}

	const float FOV = 60.0f;
	const float FOVRad = FOV * (float)UE_PI / 360.0f;
	const FMatrix ProjectionMatrix = FPerspectiveMatrix(FOVRad, 1920, 1080, 0.01f);

	const float ScreenMultiple = FMath::Max(0.5f * ProjectionMatrix.M[0][0], 0.5f * ProjectionMatrix.M[1][1]);
	const float SphereRadius = InStaticMesh->GetBounds().SphereRadius;

	InStaticMesh->Modify();
	InStaticMesh->SetNumSourceModels(1);
	InStaticMesh->GetSourceModel(0).ReductionSettings = InInfos[0].ReductionSettings;
	InStaticMesh->GetSourceModel(0).ScreenSize = InInfos[0].ScreenSize;
	if (InInfos[0].bUseDistance) {
		if (InInfos[0].Distance == 0) {
			InStaticMesh->GetSourceModel(0).ScreenSize = 2.0f ;
		}
		else {
			InStaticMesh->GetSourceModel(0).ScreenSize = 2.0f * ScreenMultiple * SphereRadius / FMath::Max(1.0f, InInfos[0].Distance);
		}
	}

	int32 LODIndex = 1;
	for (; LODIndex < InInfos.Num(); ++LODIndex){
		FStaticMeshSourceModel& SrcModel = InStaticMesh->AddSourceModel();

		SrcModel.BuildSettings = InInfos[LODIndex].BuildSettings;
		SrcModel.ReductionSettings = InInfos[LODIndex].ReductionSettings;
		SrcModel.ScreenSize = InInfos[LODIndex].ScreenSize;
		if (InInfos[LODIndex].bUseDistance) {
			if (InInfos[LODIndex].Distance == 0) {
				SrcModel.ScreenSize = 2.0f;
			}
			else {
				SrcModel.ScreenSize = 2.0f * ScreenMultiple * SphereRadius / FMath::Max(1.0f, InInfos[LODIndex].Distance);
			}
		}

		// Stop when reaching maximum of supported LODs
		if (InStaticMesh->GetNumSourceModels() == MAX_STATIC_MESH_LODS){
			break;
		}
	}
	InStaticMesh->bAutoComputeLODScreenSize = 0;
	InStaticMesh->PostEditChange();
}

float UProceduralContentProcessorLibrary::GetLodScreenSize(UStaticMesh* InStaticMesh, int32 LODIndex)
{
	if (InStaticMesh == nullptr || LODIndex< 0 || LODIndex >= InStaticMesh->GetNumLODs() || InStaticMesh->GetRenderData() == nullptr)
		return 0;
	return InStaticMesh->GetRenderData()->ScreenSize[LODIndex].GetValue();
}

float UProceduralContentProcessorLibrary::GetLodDistance(UStaticMesh* InStaticMesh, int32 LODIndex)
{
	if(LODIndex == 0)
		return 0;

	const float FOV = 60.0f;
	float ScreenSize = GetLodScreenSize(InStaticMesh, LODIndex);
	const float FOVRad = FOV * (float)UE_PI / 360.0f;
	const FMatrix ProjectionMatrix = FPerspectiveMatrix(FOVRad, 1920, 1080, 0.01f);

	const float ScreenMultiple = FMath::Max(0.5f * ProjectionMatrix.M[0][0], 0.5f * ProjectionMatrix.M[1][1]);
	const float ScreenRadius = FMath::Max(UE_SMALL_NUMBER, ScreenSize * 0.5f);

	return ComputeBoundsDrawDistance(ScreenSize, InStaticMesh->GetBounds().SphereRadius, ProjectionMatrix);
}

TArray<TSharedPtr<FSlowTask>> UProceduralContentProcessorLibrary::SlowTasks;

#undef LOCTEXT_NAMESPACE