#pragma once

#include "CoreMinimal.h"
#include "ProceduralObjectMatrix.h"
#include "ProceduralContentProcessorLibrary.generated.h"

class ALandscape;
class UStaticMeshEditorSubsystem;

UENUM(BlueprintType)
enum class EStaticMeshPivotType: uint8
{
	NoAction,
	Center,
	Bottom,
	Top,
	Left,
	Right,
	Front,
	Back,
	WorldOrigin
};

UENUM(BlueprintType)
enum class EMsgBoxType: uint8
{
	Ok = EAppMsgType::Ok,
	YesNo,
	OkCancel,
	YesNoCancel,
	CancelRetryContinue,
	YesNoYesAllNoAll,
	YesNoYesAllNoAllCancel,
	YesNoYesAll,
};

UENUM(BlueprintType)
enum class EMsgBoxReturnType: uint8
{
	No = EAppReturnType::No,
	Yes,
	YesAll,
	NoAll,
	Cancel,
	Ok,
	Retry,
	Continue,
};

USTRUCT(BlueprintType)
struct FStaticMeshLODInfo
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadWrite)
	bool bUseDistance;

	UPROPERTY(BlueprintReadWrite)
	bool bEnableBuildSetting;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditConditionHides, HideEditConditionToggle, EditCondition = "!bUseDistance"))
	float ScreenSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (HideEditConditionToggle, EditCondition = "bUseDistance"))
	float Distance;

	UPROPERTY(EditAnywhere, Category=BuildSettings, meta = (EditConditionHides, HideEditConditionToggle, EditCondition = "bEnableBuildSetting"))
	FMeshBuildSettings BuildSettings;

	/** Reduction settings to apply when building render data. */
	UPROPERTY(EditAnywhere)
	FMeshReductionSettings ReductionSettings; 
};


UCLASS()
class PROCEDURALCONTENTPROCESSOR_API UProceduralContentProcessorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	// Procedural Object Matrix Interface:

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static void ClearObjectMaterix(UPARAM(ref) FProceduralObjectMatrix& Matrix);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static void AddPropertyField(UPARAM(ref) FProceduralObjectMatrix& Matrix, UObject* InObject, FName InPropertyName);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static void AddPropertyFieldWithOwner(UPARAM(ref) FProceduralObjectMatrix& Matrix, UObject* InOwner, UObject* InObject, FName InPropertyName);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static void AddTextField(UPARAM(ref) FProceduralObjectMatrix& Matrix, UObject* InObject, FName InFieldName, FString InFieldValue);


	// Object Interface:

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static TArray<UClass*> GetDerivedClasses(const UClass* ClassToLookFor, bool bRecursive);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor", meta = (DefaultToSelf = "Outer"))
	static UObject* DuplicateObject(UObject* SourceObject, UObject* Outer);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static int32 DeleteObjects(const TArray< UObject* >& ObjectsToDelete, bool bShowConfirmation = true, bool bAllowCancelDuringDelete = true);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static int32 DeleteObjectsUnchecked(const TArray< UObject* >& ObjectsToDelete);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static void ConsolidateObjects(UObject* ObjectToConsolidateTo, UPARAM(ref) TArray<UObject*>& ObjectsToConsolidate, bool bShowDeleteConfirmation = true);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static TSet<UObject*> GetAssetReferences(UObject* Object, const TArray<UClass*>& IgnoreClasses, bool bIncludeDefaultRefs = false);
	

	// Editor Interface:

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static void BeginTransaction(FText Text);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static void ModifyObject(UObject* Object);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static void EndTransaction();

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static UUserWidget* AddDialog(UObject* Outer, TSubclassOf<UUserWidget> WidgetClass, FText WindowTitle = INVTEXT("Dialog"), FVector2D DialogSize = FVector2D(400, 300), bool IsModalWindow = true);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static EMsgBoxReturnType AddMessageBox(EMsgBoxType Type, FText Msg);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor", meta = (DefaultToSelf = "ParentWidget"))
	static UUserWidget* AddContextMenu(UUserWidget* ParentWidget, TSubclassOf<UUserWidget> WidgetClass, FVector2D SummonLocation = FVector2D::ZeroVector);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static void DismissAllMenus();

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static void AddNotification(FText Notification, float FadeInDuration = 2, float ExpireDuration = 2, float FadeOutDuration = 2);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static void AddNotificationWithButton(FText Notification, UObject* Object, TArray<FName> FunctionNames);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static void PushSlowTask(FText TaskName, float AmountOfWork);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static void EnterSlowTaskProgressFrame(float ExpectedWorkThisFrame = 1.f, const FText& Text = FText());

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static void PopSlowTask();

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static void ClearAllSlowTask();

	// Asset Interface:
	
	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static bool IsNaniteEnable(UStaticMesh* InMesh);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static void SetNaniteMeshEnabled(UStaticMesh* InMesh, bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static void GenerateStaticMeshLODs(UStaticMesh* InMesh, FName LODGroup, int LODCount);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static bool IsMaterialHasTimeNode(AStaticMeshActor* InActor);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static bool MatchString(FString InString,const TArray<FString>& IncludeList, const TArray<FString>& ExcludeList);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static bool IsGeneratedByBlueprint(UObject* InObject);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static UObject* CopyProperties(UObject* OldObject, UObject* NewObject);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static UStaticMesh* GetComplexCollisionMesh(UStaticMesh* InMesh);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static void SetStaticMeshPivot(UStaticMesh* InStaticMesh, EStaticMeshPivotType PivotType);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "ProceduralContentProcessor")
	static UStaticMeshEditorSubsystem* GetStaticMeshEditorSubsystem();

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static TArray<FStaticMeshLODInfo> GetStaticMeshLODInfos(UStaticMesh* InStaticMesh, bool bUseDistance = false, bool bEnableBuildSetting = false);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static void SetStaticMeshLODInfos(UStaticMesh* InStaticMesh, TArray<FStaticMeshLODInfo> InInfos);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static float GetLodScreenSize(UStaticMesh* InStaticMesh, int32 LODIndex);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static float GetLodDistance(UStaticMesh* InStaticMesh, int32 LODIndex);

	static TArray<TSharedPtr<FSlowTask>> SlowTasks;
};
