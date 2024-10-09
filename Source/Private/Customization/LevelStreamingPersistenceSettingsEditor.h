#pragma once
#include "ProceduralContentProcessor.h"
#include "IDetailCustomization.h"
#include "IPropertyTypeCustomization.h"
#include "LevelStreamingPersistenceSettings.h"
#include "LevelStreamingPersistenceSettingsEditor.generated.h"

class FDetailCustomization_LevelStreamingPersistenceEditorSettings : public IDetailCustomization {
public:
	static TSharedRef<IDetailCustomization> MakeInstance();
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};

UCLASS()
class ULevelStreamingPersistentPropertyProxy: public UObject {
	GENERATED_BODY()
public:
	UPROPERTY()
	TSoftClassPtr<UObject> Class;

	UPROPERTY()
	FString PropertyName;

	UPROPERTY()
	bool bPublic = false;
}; 

class FPropertyTypeCustomization_LevelStreamingPersistentProperty : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& InHeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	void OnProxyClassPropertyChanged();
	void OnProxyPublicPropertyChanged();
	void OnProxyPropertyNameChanged(TSharedPtr<FString> InText, ESelectInfo::Type InType);
private:
	TStrongObjectPtr<ULevelStreamingPersistentPropertyProxy> mPropertyProxy;
	TSharedPtr<IPropertyHandle> mPropertyHandle;
	FLevelStreamingPersistentProperty* mPropertyPtr = nullptr;
	TArray<TSharedPtr<FString>> mPropertyList;
	TSharedPtr<STextComboBox> mPropertyComboBox;
};

UCLASS(EditInlineNew, CollapseCategories, config = ProceduralContentProcessor, defaultconfig, Category = "WorldPartition")
class PROCEDURALCONTENTPROCESSOR_API ULevelStreamingPersistenceEditor : public UProceduralWorldProcessor {
	GENERATED_BODY()
protected:
	virtual TSharedPtr<SWidget> BuildWidget() override;
};