#include "ProceduralPropertyMatrixCustomization.h"
#include "Kismet/GameplayStatics.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "Kismet2/CompilerResultsLog.h"
#include "PropertyCustomizationHelpers.h"
#include "IPropertyUtilities.h"
#include "Selection.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Input/SSearchBox.h"
#include "ISinglePropertyView.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"

#define LOCTEXT_NAMESPACE "ProceduralContentProcessor"

class SProceduralPropertyMatrixInfoViewRow
	: public SMultiColumnTableRow<TSharedPtr<FProceduralPropertyMatrixInfo>> {
public:
	SLATE_BEGIN_ARGS(SProceduralPropertyMatrixInfoViewRow) {}
	SLATE_ARGUMENT(TSharedPtr<FProceduralPropertyMatrixInfo>, MatrixInfo)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		MatrixInfo = InArgs._MatrixInfo;
		SMultiColumnTableRow<TSharedPtr<FProceduralPropertyMatrixInfo>>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override {
		if (ColumnName == "Name" && MatrixInfo->Object.IsValid()) {
			return SNew(SBox)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.Padding(4)
				[
					SNew(STextBlock)
					.Text(FText::FromString(MatrixInfo->Object->GetName()))
				];
		}		
		TPair<FName, FString> FieldData;
		FieldData.Key = ColumnName;
		for (auto Field : MatrixInfo->Fields) {
			if (Field.Key == FieldData.Key) {
				FieldData = Field;
				break;
			}
		}
		if (FieldData.Value.IsEmpty()) {
			if (MatrixInfo->Object != nullptr) {
				FName PropertyName = ColumnName;
				if (MatrixInfo->Object->GetClass()->IsValidLowLevel()) {
					FProperty* Property = FindFProperty<FProperty>(MatrixInfo->Object->GetClass(), PropertyName);
					if (Property) {
						FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
						FSinglePropertyParams Params;
						Params.NamePlacement = EPropertyNamePlacement::Hidden;
						auto Widget = EditModule.CreateSingleProperty(MatrixInfo->Object.Get(), PropertyName, Params);
						Widget->SetEnabled(!Property->HasAnyPropertyFlags(CPF_DisableEditOnInstance));
						return Widget.ToSharedRef();
					}
				}
			}
		}
		return SNew(SBox)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(4)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FieldData.Value))
			];
	}
private:
	TSharedPtr<FProceduralPropertyMatrixInfo> MatrixInfo;
};

TSharedRef<IPropertyTypeCustomization> FPropertyTypeCustomization_ProceduralPropertyMatrix::MakeInstance()
{
	return MakeShared<FPropertyTypeCustomization_ProceduralPropertyMatrix>();
}

FPropertyTypeCustomization_ProceduralPropertyMatrix::FPropertyTypeCustomization_ProceduralPropertyMatrix()
{
}

FPropertyTypeCustomization_ProceduralPropertyMatrix::~FPropertyTypeCustomization_ProceduralPropertyMatrix()
{
}

void FPropertyTypeCustomization_ProceduralPropertyMatrix::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& InHeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	void* Ptr = nullptr;
	InPropertyHandle->GetValueData(Ptr);
	ProceduralPropertyMatrix = (FProceduralPropertyMatrix*)Ptr;
	InHeaderRow
	.WholeRowContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
			.VAlign(EVerticalAlignment::VAlign_Center)
			.Padding(5)
			[
				SNew(SSearchBox)
				.OnTextCommitted(this, &FPropertyTypeCustomization_ProceduralPropertyMatrix::OnSearchBoxTextCommitted)
				.HintText(LOCTEXT("Search", "Search..."))
			]
	];
}

void FPropertyTypeCustomization_ProceduralPropertyMatrix::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	ChildBuilder.AddCustomRow(FText::FromString("List"))
	.Visibility(TAttribute<EVisibility>(this, &FPropertyTypeCustomization_ProceduralPropertyMatrix::GetVisibility))
	.ShouldAutoExpand()
	.WholeRowContent()
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Top)
	[
		SAssignNew(ListViewContainer, SBox)
		.MaxDesiredHeight(800)
	];
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateSP(this, &FPropertyTypeCustomization_ProceduralPropertyMatrix::OnTick));
}

EVisibility FPropertyTypeCustomization_ProceduralPropertyMatrix::GetVisibility() const
{
	return (ProceduralPropertyMatrix&&ProceduralPropertyMatrix->ObjectInfoList.IsEmpty())? EVisibility::Collapsed : EVisibility::Visible;
}

EColumnSortMode::Type FPropertyTypeCustomization_ProceduralPropertyMatrix::GetColumnSortMode(const FName ColumnId) const
{
	if (ColumnId == ProceduralPropertyMatrix->SortedColumnName)
	{
		return ProceduralPropertyMatrix->SortMode;
	}
	return EColumnSortMode::None;
}

void FPropertyTypeCustomization_ProceduralPropertyMatrix::OnSort(EColumnSortPriority::Type InPriorityType, const FName& InName, EColumnSortMode::Type InType)
{
	ProceduralPropertyMatrix->SortedColumnName = InName;
	ProceduralPropertyMatrix->SortMode = InType;

	TFunction<bool(bool)> SortType = [](bool Var) {return Var; };
	if (InType == EColumnSortMode::Descending) {
		SortType = [](bool Var) {return !Var; };
	}
	ProceduralPropertyMatrix->ObjectInfoList.StableSort([&](const TSharedPtr<FProceduralPropertyMatrixInfo>& Lhs, const TSharedPtr<FProceduralPropertyMatrixInfo>& Rhs) {
		FString LhsValue;
		{
			UObject* Object = Lhs->Object.GetEvenIfUnreachable();
			if (Object != nullptr) {
				FProperty* Property = FindFProperty<FProperty>(Object->GetClass(), InName);
				if (Property) {
					FBlueprintEditorUtils::PropertyValueToString(Property, reinterpret_cast<const uint8*>(Object), LhsValue);
				}
			}
			if (LhsValue.IsEmpty()) {
				for (auto Field : Lhs->Fields) {
					if (Field.Key == InName) {
						LhsValue = Field.Value;
						break;
					}
				}
			}
		}
		FString RhsValue;
		{
			UObject* Object = Rhs->Object.GetEvenIfUnreachable();
			if (Object != nullptr) {
				FProperty* Property = FindFProperty<FProperty>(Object->GetClass(), InName);
				if (Property) {
					FBlueprintEditorUtils::PropertyValueToString(Property, reinterpret_cast<const uint8*>(Object), RhsValue);
				}
			}
			if (RhsValue.IsEmpty()) {
				for (auto Field : Rhs->Fields) {
					if (Field.Key == InName) {
						RhsValue = Field.Value;
						break;
					}
				}
			}
		}
		if (LhsValue.IsNumeric() && RhsValue.IsNumeric())
			return SortType(FCString::Atod(*LhsValue) < FCString::Atod(*RhsValue));
		return SortType(LhsValue < RhsValue);
	});

	ProceduralPropertyMatrix->ObjectInfoListView->RequestListRefresh();
}

TSharedRef<ITableRow> FPropertyTypeCustomization_ProceduralPropertyMatrix::OnGenerateRow(TSharedPtr<FProceduralPropertyMatrixInfo> InInfo, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SProceduralPropertyMatrixInfoViewRow, OwnerTable)
		.MatrixInfo(InInfo);
}

void FPropertyTypeCustomization_ProceduralPropertyMatrix::OnMouseButtonDoubleClick(TSharedPtr<FProceduralPropertyMatrixInfo> InInfo)
{
	if (InInfo) {
		if (auto Actor = Cast<AActor>(InInfo->Object)) {
			GEditor->GetSelectedActors()->Modify();
			GEditor->GetSelectedActors()->BeginBatchSelectOperation();
			GEditor->SelectNone(false, true, true);
			GEditor->SelectActor(Actor, true, false, true);
			GEditor->MoveViewportCamerasToActor({ Actor }, true);
			GEditor->GetSelectedActors()->EndBatchSelectOperation(/*bNotify*/false);
			GEditor->NoteSelectionChange();
		}
		else if (InInfo->Object->IsAsset()) {
			FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			TArray<UObject*> SyncObjects;
			SyncObjects.Add(InInfo->Object.Get());
			ContentBrowserModule.Get().SyncBrowserToAssets(SyncObjects, true);
		}
	}
}

void FPropertyTypeCustomization_ProceduralPropertyMatrix::OnSearchBoxTextCommitted(const FText& InNewText, ETextCommit::Type InTextCommit) {
	if (InNewText.IsEmpty()) {
		ProceduralPropertyMatrix->ObjectInfoListView->SetListItemsSource(ProceduralPropertyMatrix->ObjectInfoList);
		CurrInfoList = &ProceduralPropertyMatrix->ObjectInfoList;
		CurrentSearchKeyword = FString();
	}
	else {
		SearchInfoList.Reset();
		CurrentSearchKeyword = InNewText.ToString();
		for (auto& ObjectInfo : ProceduralPropertyMatrix->ObjectInfoList) {
			if (ObjectInfo->Object.IsValid()) {
				if (ObjectInfo->Object->GetName().Contains(CurrentSearchKeyword)) {
					SearchInfoList.Add(ObjectInfo);
				}
				else {
					for (const auto& Field : ObjectInfo->Fields) {
						if (Field.Value.Contains(CurrentSearchKeyword)) {
							SearchInfoList.Add(ObjectInfo);
							break;
						}
					}
				}
			}
		}
		ProceduralPropertyMatrix->ObjectInfoListView->SetListItemsSource(SearchInfoList);
		CurrInfoList = &SearchInfoList;
	}
	ProceduralPropertyMatrix->ObjectInfoListView->RebuildList();
}

void FPropertyTypeCustomization_ProceduralPropertyMatrix::RebuildListView()
{
	TSharedPtr<SHeaderRow> HeaderRow;
	auto View = SAssignNew(ProceduralPropertyMatrix->ObjectInfoListView, SListView<TSharedPtr<FProceduralPropertyMatrixInfo>>)
		.ScrollbarVisibility(EVisibility::Visible)
		.ListItemsSource(&ProceduralPropertyMatrix->ObjectInfoList)
		.OnGenerateRow_Raw(this, &FPropertyTypeCustomization_ProceduralPropertyMatrix::OnGenerateRow)
		.OnMouseButtonDoubleClick_Raw(this, &FPropertyTypeCustomization_ProceduralPropertyMatrix::OnMouseButtonDoubleClick)
		.HeaderRow(
			SAssignNew(HeaderRow, SHeaderRow)
			.ResizeMode(ESplitterResizeMode::FixedPosition)
			.CanSelectGeneratedColumn(true)
		);
	HeaderRow->AddColumn(
		SHeaderRow::Column("Name")
		.HAlignHeader(EHorizontalAlignment::HAlign_Center)
		.DefaultLabel(LOCTEXT("Name","Name"))
		.SortMode_Raw(this, &FPropertyTypeCustomization_ProceduralPropertyMatrix::GetColumnSortMode, FName("Name"))
		.SortPriority(EColumnSortPriority::Primary)
		.OnSort_Raw(this, &FPropertyTypeCustomization_ProceduralPropertyMatrix::OnSort)
	);
	for (auto FieldKey : ProceduralPropertyMatrix->FieldKeys) {
		if (!FieldKey.IsNone()) {
			HeaderRow->AddColumn(
				SHeaderRow::Column(FieldKey)
				.HAlignHeader(EHorizontalAlignment::HAlign_Center)
				.DefaultLabel(FText::FromName(FieldKey))
				.SortMode_Raw(this, &FPropertyTypeCustomization_ProceduralPropertyMatrix::GetColumnSortMode, FieldKey)
				.SortPriority(EColumnSortPriority::Primary)
				.OnSort_Raw(this, &FPropertyTypeCustomization_ProceduralPropertyMatrix::OnSort)
			);
		}
	}
	ListViewContainer->SetContent(View);
}

bool FPropertyTypeCustomization_ProceduralPropertyMatrix::OnTick(float Delta)
{
	if (ProceduralPropertyMatrix && ProceduralPropertyMatrix->bIsDirty) {
		ProceduralPropertyMatrix->bIsDirty = false;
		RebuildListView();
	}
	return true;
}

#undef LOCTEXT_NAMESPACE