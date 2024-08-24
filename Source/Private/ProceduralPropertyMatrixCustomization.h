#pragma once

#include "ProceduralPropertyMatrix.h"
#include "IPropertyTypeCustomization.h"

struct FProceduralPropertyMatrixInfo;

class FPropertyTypeCustomization_ProceduralPropertyMatrix
	: public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	FPropertyTypeCustomization_ProceduralPropertyMatrix();

	~FPropertyTypeCustomization_ProceduralPropertyMatrix();

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& InHeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)override;

	EVisibility GetVisibility() const;
	EColumnSortMode::Type GetColumnSortMode(const FName ColumnId) const;
	void OnSort(EColumnSortPriority::Type InPriorityType, const FName& InName, EColumnSortMode::Type InType);
	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FProceduralPropertyMatrixInfo> InInfo, const TSharedRef<STableViewBase>& OwnerTable);
	void OnMouseButtonDoubleClick(TSharedPtr<FProceduralPropertyMatrixInfo> InInfo);
	void OnSearchBoxTextCommitted(const FText& InNewText, ETextCommit::Type InTextCommit);
	void RebuildListView();
	bool OnTick(float Delta);
private:
	TSharedPtr<IPropertyUtilities> Utils;
	TSharedPtr<IPropertyHandle> ModulesHandle;
	FProceduralPropertyMatrix* ProceduralPropertyMatrix;
	TArray<TSharedPtr<FProceduralPropertyMatrixInfo>> SearchInfoList;
	TArray<TSharedPtr<FProceduralPropertyMatrixInfo>>* CurrInfoList = nullptr;
	FString CurrentSearchKeyword;
	TSharedPtr<SBox> ListViewContainer;
};