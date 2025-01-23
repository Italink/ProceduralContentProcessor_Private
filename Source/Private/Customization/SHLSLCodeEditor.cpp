// Copyright Epic Games, Inc. All Rights Reserved.

#include "SHLSLCodeEditor.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SButton.h"
#include "ISequencer.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SScrollBox.h"
#include "UObject/Class.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Widgets/Docking/SDockTab.h"
#include "Text/HLSLSyntaxHighlighterMarshaller.h"
#include "NiagaraEditorStyle.h"

#define LOCTEXT_NAMESPACE "ProceduralContentProcessor"

void SHLSLCodeEditor::Construct(const FArguments& InArgs, FText InHLSLCode)
{
	SyntaxHighlighter = FHLSLSyntaxHighlighterMarshaller::Create(
		FHLSLSyntaxHighlighterMarshaller::FSyntaxTextStyle(FNiagaraEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("SyntaxHighlight.HLSL.Normal"),
		FNiagaraEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("SyntaxHighlight.HLSL.Operator"),
		FNiagaraEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("SyntaxHighlight.HLSL.Keyword"),
		FNiagaraEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("SyntaxHighlight.HLSL.String"),
		FNiagaraEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("SyntaxHighlight.HLSL.Number"),
		FNiagaraEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("SyntaxHighlight.HLSL.Comment"),
		FNiagaraEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("SyntaxHighlight.HLSL.PreProcessorKeyword"),
		FNiagaraEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("SyntaxHighlight.HLSL.Error")));

	TSharedRef<SWidget> HeaderContentsFirstLine = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			SNullWidget::NullWidget
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		.Padding(2, 4, 2, 4)
		[
			SAssignNew(SearchBox, SSearchBox)
			.OnTextCommitted(this, &SHLSLCodeEditor::OnSearchTextCommitted)
			.HintText(NSLOCTEXT("SearchBox", "HelpHint", "Search For Text"))
			.OnTextChanged(this, &SHLSLCodeEditor::OnSearchTextChanged)
			.SelectAllTextWhenFocused(false)
			.DelayChangeNotificationsWhileTyping(true)
			.MinDesiredWidth(200)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(2, 4, 2, 4)
		[
			SAssignNew(SearchFoundMOfNText, STextBlock)
			.MinDesiredWidth(25)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2, 4, 2, 4)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
			.IsFocusable(false)
			.ToolTipText(LOCTEXT("UpToolTip", "Focus to previous found search term"))
			.OnClicked(this, &SHLSLCodeEditor::SearchUpClicked)
			.Content()
			[
				SNew(STextBlock)
				.Font(FAppStyle::Get().GetFontStyle("FontAwesome.10"))
				.Text(FText::FromString(FString(TEXT("\xf062"))))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2, 4, 2, 4)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
			.IsFocusable(false)
			.ToolTipText(LOCTEXT("DownToolTip", "Focus to next found search term"))
			.OnClicked(this, &SHLSLCodeEditor::SearchDownClicked)
			.Content()
			[
				SNew(STextBlock)
				.Font(FAppStyle::Get().GetFontStyle("FontAwesome.10"))
				.Text(FText::FromString(FString(TEXT("\xf063"))))
			]
		];

	this->ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight() // Header block
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
			[
				HeaderContentsFirstLine
			]
		]
		+ SVerticalBox::Slot() // Text body block
		[
			SAssignNew(TextBodyContainer, SVerticalBox)
		]
	]; 
	
	if (!GeneratedCode.HorizontalScrollBar.IsValid())
	{
		GeneratedCode.HorizontalScrollBar = SNew(SScrollBar)
			.Orientation(Orient_Horizontal)
			.Thickness(FVector2D(12.0f, 12.0f));
	}

	if (!GeneratedCode.VerticalScrollBar.IsValid())
	{
		GeneratedCode.VerticalScrollBar = SNew(SScrollBar)
			.Orientation(Orient_Vertical)
			.Thickness(FVector2D(12.0f, 12.0f));
	}

	if (!GeneratedCode.Container.IsValid())
	{
		SAssignNew(GeneratedCode.Container, SVerticalBox)
			+ SVerticalBox::Slot()
			[
				SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SAssignNew(GeneratedCode.Text, SMultiLineEditableTextBox)
							.Marshaller(SyntaxHighlighter)
							//.SearchText(this, &SHLSLCodeEditor::GetSearchText)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						GeneratedCode.VerticalScrollBar.ToSharedRef()
					]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				GeneratedCode.HorizontalScrollBar.ToSharedRef()
			];
	}

	GeneratedCode.Text->SetText(InHLSLCode);

	TextBodyContainer->AddSlot()
		[
			GeneratedCode.Container.ToSharedRef()
		];


	DoSearch(SearchBox->GetText());
}

void SHLSLCodeEditor::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}

FReply SHLSLCodeEditor::SearchDownClicked()
{
	if (ActiveFoundTextEntries.Num() > 0)
	{
		CurrentFoundTextEntry++;
		if (CurrentFoundTextEntry == ActiveFoundTextEntries.Num())
		{
			CurrentFoundTextEntry = 0;
		}
	}
	GeneratedCode.Text->AdvanceSearch(false);

	SetSearchMofN();

	return FReply::Handled();
}

FReply SHLSLCodeEditor::SearchUpClicked()
{
	if (ActiveFoundTextEntries.Num() > 0)
	{
		CurrentFoundTextEntry--;
		if (CurrentFoundTextEntry < 0)
		{
			CurrentFoundTextEntry = ActiveFoundTextEntries.Num() - 1;
		}
	}
	GeneratedCode.Text->AdvanceSearch(true);
	
	SetSearchMofN();

	return FReply::Handled();
}

void SHLSLCodeEditor::OnSearchTextChanged(const FText& InFilterText)
{
	DoSearch(InFilterText);
}

void SHLSLCodeEditor::DoSearch(const FText& InFilterText)
{
	const FText OldText = GeneratedCode.Text->GetSearchText();
	GeneratedCode.Text->SetSearchText(InFilterText);
	GeneratedCode.Text->BeginSearch(InFilterText, ESearchCase::IgnoreCase, false);

	FString SearchString = InFilterText.ToString();
	ActiveFoundTextEntries.Empty();
	if (SearchString.IsEmpty())
	{
		SetSearchMofN();
		return;
	}

	ActiveFoundTextEntries.Empty();
	//for (int32 i = 0; i < GeneratedCode.HlslByLines.Num(); i++)
	//{
	//	const FString& Line = GeneratedCode.HlslByLines[i];
	//	int32 FoundPos = Line.Find(SearchString, ESearchCase::IgnoreCase);
	//	while (FoundPos != INDEX_NONE && ActiveFoundTextEntries.Num() < 1000) // guard against a runaway loop
	//	{
	//		ActiveFoundTextEntries.Add(FTextLocation(i, FoundPos));
	//		int32 LastPos = FoundPos + SearchString.Len();
	//		if (LastPos < Line.Len())
	//		{
	//			FoundPos = Line.Find(SearchString, ESearchCase::IgnoreCase, ESearchDir::FromStart, LastPos);
	//		}
	//		else
	//		{
	//			FoundPos = INDEX_NONE;
	//		}
	//	}
	//}

	//if (ActiveFoundTextEntries.Num() > 0 && OldText.CompareTo(InFilterText) != 0)
	//{
	//	CurrentFoundTextEntry = 0;
	//	//GeneratedCode[TabState].Text->ScrollTo(ActiveFoundTextEntries[CurrentFoundTextEntry]);
	//}
	//else if (ActiveFoundTextEntries.Num() == 0)
	//{
	//	CurrentFoundTextEntry = INDEX_NONE;
	//}

	SetSearchMofN();
}

void SHLSLCodeEditor::SetSearchMofN()
{
	SearchFoundMOfNText->SetText(FText::Format(LOCTEXT("MOfN", "{0} of {1}"), FText::AsNumber(CurrentFoundTextEntry + 1), FText::AsNumber(ActiveFoundTextEntries.Num())));
	//SearchFoundMOfNText->SetText(FText::Format(LOCTEXT("MOfN", "{1} found"), FText::AsNumber(CurrentFoundTextEntry), FText::AsNumber(ActiveFoundTextEntries.Num())));
}

void SHLSLCodeEditor::OnSearchTextCommitted(const FText& InFilterText, ETextCommit::Type InCommitType)
{
	OnSearchTextChanged(InFilterText);
	if (ActiveFoundTextEntries.Num() > 0)
	{
		CurrentFoundTextEntry++;
		if (CurrentFoundTextEntry == ActiveFoundTextEntries.Num())
		{
			CurrentFoundTextEntry = 0;
		}
	}

	GeneratedCode.Text->AdvanceSearch(true);

	SetSearchMofN();
}

SHLSLCodeEditor::~SHLSLCodeEditor()
{
}

void SHLSLCodeEditor::SetCode(FText InCode)
{
	GeneratedCode.Text->SetText(InCode);
}

FText SHLSLCodeEditor::GetCode()
{
	return GeneratedCode.Text->GetText();
}

FText SHLSLCodeEditor::GetSearchText() const
{
	return SearchBox->GetText();
}

#undef LOCTEXT_NAMESPACE
