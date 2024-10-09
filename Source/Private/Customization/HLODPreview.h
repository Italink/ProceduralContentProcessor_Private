#pragma once

#include "ProceduralContentProcessor.h"
#include "HLODPreview.generated.h"

UCLASS(EditInlineNew, CollapseCategories, config = ProceduralContentProcessor, defaultconfig, Category = "WorldPartition", meta = (DisplayName = "HLOD Preview"))
class PROCEDURALCONTENTPROCESSOR_API UHLODPreview: public UProceduralWorldProcessor {
	GENERATED_BODY()
protected:
	virtual TSharedPtr<SWidget> BuildWidget() override;
private:
	TSharedPtr<SWidget> HLODOutliner;
};