#pragma once

#include "ProceduralContentProcessor.h"
#include "Engine/StaticMeshActor.h"
#include "VertexColorProcessor.generated.h"

class SHLSLCodeEditor;

UCLASS(EditInlineNew, CollapseCategories, config = ProceduralContentProcessor, defaultconfig)
class PROCEDURALCONTENTPROCESSOR_API UVertexColorProcessor: public UProceduralWorldProcessor {
	GENERATED_BODY()
public:
	UVertexColorProcessor();
protected:
	virtual void Activate() override;
	virtual void Deactivate() override;

	virtual TSharedPtr<SWidget> BuildWidget() override;

	void OnActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh);
	FReply OnClickedReset();
	FReply OnClickedExecute();

	void OnApplyVertexColor(TArray<FVector4f> VertexColors);
private:
	FText DefaultCS;

	FDelegateHandle OnActorSelectionChangedHandle;

	TSharedPtr<SHLSLCodeEditor> HLSLEditor;

	UPROPERTY(Config)
	FString CurrentCode;

	UPROPERTY()
	TObjectPtr<AStaticMeshActor> StaticMeshActor;
};