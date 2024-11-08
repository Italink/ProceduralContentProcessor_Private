#pragma once

#include "ProceduralContentProcessor.h"
#include "HLODPreviewTool.generated.h"

UCLASS(EditInlineNew, CollapseCategories, config = ProceduralContentProcessor, defaultconfig, Category = "WorldPartition", meta = (DisplayName = "HLOD Preview Tool"))
class PROCEDURALCONTENTPROCESSOR_API UHLODPreviewTool: public UProceduralWorldProcessor {
	GENERATED_BODY()
protected:
	virtual TSharedPtr<SWidget> BuildWidget() override;
private:
	TSharedPtr<SWidget> HLODOutliner;
};