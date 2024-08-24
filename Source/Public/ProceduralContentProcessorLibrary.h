#pragma once

#include "CoreMinimal.h"
#include "ProceduralPropertyMatrix.h"
#include "ProceduralContentProcessorLibrary.generated.h"

class ALandscape;

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

UCLASS()
class PROCEDURALCONTENTPROCESSOR_API UProceduralContentProcessorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static void ClearPropertyMaterix(UPARAM(ref) FProceduralPropertyMatrix& Matrix);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static void AddPropertyMaterixField(UPARAM(ref) FProceduralPropertyMatrix& Matrix, UObject* InObject, FName InPropertyName);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static void AddPropertyMaterixFieldValue(UPARAM(ref) FProceduralPropertyMatrix& Matrix, UObject* InObject, FName InFieldName, FString InFieldValue);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static void BeginTransaction(FText Text);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static void ModifyObject(UObject* Object);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static void EndTransaction();

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor", meta = (DefaultToSelf = "Outer"))
	static UObject* DuplicateObject(UObject* SourceObject, UObject* Outer);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor", meta = (DefaultToSelf = "Outer"))
	static UUserWidget* PushDialog(UObject* Outer, TSubclassOf<UUserWidget> WidgetClass, FText WindowTitle = INVTEXT("Dialog"), FVector2D DialogSize = FVector2D(400, 300), bool IsModalWindow = true);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor", meta = (DefaultToSelf = "Outer"))
	static EMsgBoxReturnType PushMsgBox(EMsgBoxType Type, FText Msg);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor", meta = (DefaultToSelf = "ParentWidget"))
	static UUserWidget* PushContextMenu(UUserWidget* ParentWidget, TSubclassOf<UUserWidget> WidgetClass, FVector2D SummonLocation = FVector2D::ZeroVector);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static void DismissAllMenus();

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static bool IsNaniteEnable(UStaticMesh* InMesh);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static void GenerateStaticMeshLODs(UStaticMesh* InMesh, FName LODGroup, int LODCount);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static bool IsDynamicMaterial(AActor* InActor);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static bool MatchString(FString InString,const TArray<FString>& IncludeList, const TArray<FString>& ExcludeList);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static void SetNaniteMeshEnabled(UStaticMesh* InMesh, bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static bool IsGeneratedByBlueprint(UObject* InObject);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static UObject* CopyProperties(UObject* OldObject, UObject* NewObject);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static UStaticMesh* GetComplexCollisionMesh(UStaticMesh* InMesh);

	UFUNCTION(BlueprintCallable, Category = "ProceduralContentProcessor")
	static void SetStaticMeshPivot(UStaticMesh* InStaticMesh, EStaticMeshPivotType PivotType);
};
