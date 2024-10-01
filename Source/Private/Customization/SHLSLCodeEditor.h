// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorUndoClient.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STreeView.h"
#include "TickableEditorObject.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Templates/SharedPointer.h"
#include "Containers/Map.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"

class FHLSLSyntaxHighlighterMarshaller;

class SHLSLCodeEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHLSLCodeEditor)
	{}

	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, FText InHLSLCode);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime);
	virtual ~SHLSLCodeEditor();

	void SetCode(FText InCode);
	FText GetCode();
protected:
	struct TabInfo
	{
		TSharedPtr<SMultiLineEditableTextBox> Text;
		TSharedPtr<SScrollBar> HorizontalScrollBar;
		TSharedPtr<SScrollBar> VerticalScrollBar;
		TSharedPtr<SVerticalBox> Container;
	};

	TabInfo GeneratedCode;
	TSharedPtr<SComboButton> ScriptNameCombo;
	TSharedPtr<SHorizontalBox> ScriptNameContainer;
	TSharedPtr<SVerticalBox> TextBodyContainer;
	TSharedPtr<SSearchBox> SearchBox;
	TSharedPtr<STextBlock> SearchFoundMOfNText;
	TArray<FTextLocation> ActiveFoundTextEntries;
	TSharedPtr<FHLSLSyntaxHighlighterMarshaller> SyntaxHighlighter;
	int32 CurrentFoundTextEntry;

	void SetSearchMofN();

	void SystemSelectionChanged();
	void OnSearchTextChanged(const FText& InFilterText);
	void OnSearchTextCommitted(const FText& InFilterText, ETextCommit::Type InCommitType);
	FReply SearchUpClicked();
	FReply SearchDownClicked();
	void DoSearch(const FText& InFilterText);
	FText GetSearchText() const;
};


