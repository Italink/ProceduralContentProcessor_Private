#include "LevelStreamingPersistenceSettingsEditor.h"
#include "DetailLayoutBuilder.h"
#include "LevelStreamingPersistenceSettings.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "ISinglePropertyView.h"
#include "Widgets/Input/STextComboBox.h"
#include "IDetailChildrenBuilder.h"

#define LOCTEXT_NAMESPACE "LevelStreamingPersistenceEditor"

TSharedRef<IDetailCustomization> FDetailCustomization_LevelStreamingPersistenceEditorSettings::MakeInstance()
{
	return MakeShared<FDetailCustomization_LevelStreamingPersistenceEditorSettings>();
}

void FDetailCustomization_LevelStreamingPersistenceEditorSettings::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	if (Objects.Num() != 1) {
		return;
	}
	ULevelStreamingPersistenceSettings* Settings = (ULevelStreamingPersistenceSettings*)Objects[0].Get();
	IDetailCategoryBuilder& CategoryBuilder = DetailBuilder.EditCategory(FName(), FText::GetEmpty(), ECategoryPriority::Uncommon);
	auto Row = CategoryBuilder.AddExternalObjectProperty({Settings},"Properties",
		EPropertyLocation::Default,
		FAddPropertyParams()
			.AllowChildren(true)
	);
}

TSharedRef<IPropertyTypeCustomization> FPropertyTypeCustomization_LevelStreamingPersistentProperty::MakeInstance()
{
	return MakeShared<FPropertyTypeCustomization_LevelStreamingPersistentProperty>();
}

void FPropertyTypeCustomization_LevelStreamingPersistentProperty::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& InHeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	void* Ptr = nullptr;
	InPropertyHandle->GetValueData(Ptr);
	mPropertyPtr = (FLevelStreamingPersistentProperty*)Ptr;
	mPropertyHandle = InPropertyHandle;

	InHeaderRow.NameContent()
		[
			mPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			mPropertyHandle->CreatePropertyValueWidget()
		];

}

void FPropertyTypeCustomization_LevelStreamingPersistentProperty::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	mPropertyProxy.Reset(NewObject<ULevelStreamingPersistentPropertyProxy>());
	mPropertyProxy->bPublic = mPropertyPtr->bIsPublic;
	//FSoftObjectPath(mPropertyPtr->Path).TryLoad();
	TSharedPtr<FString> InitiallyProperty;
	if (FProperty* Property = TFieldPath<FProperty>(*mPropertyPtr->Path).Get()) {
		mPropertyProxy->Class = Property->GetOwnerClass();
		mPropertyProxy->PropertyName = Property->GetName();
		OnProxyClassPropertyChanged();
		for (const auto& Prop : mPropertyList) {
			if (*Prop == Property->GetFName()) {
				InitiallyProperty = Prop;
				break;
			}
		}
	}

	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FSinglePropertyParams Params;
	Params.NamePlacement = EPropertyNamePlacement::Hidden;

	TSharedPtr<ISinglePropertyView> ClassView = EditModule.CreateSingleProperty(mPropertyProxy.Get(), "Class", Params);
	TSharedPtr<ISinglePropertyView> PublicView = EditModule.CreateSingleProperty(mPropertyProxy.Get(), "bPublic", Params);

	FSimpleDelegate Delegate = FSimpleDelegate::CreateSP(this, &FPropertyTypeCustomization_LevelStreamingPersistentProperty::OnProxyClassPropertyChanged);
	ClassView->SetOnPropertyValueChanged(Delegate);

	Delegate = FSimpleDelegate::CreateSP(this, &FPropertyTypeCustomization_LevelStreamingPersistentProperty::OnProxyPublicPropertyChanged);
	PublicView->SetOnPropertyValueChanged(Delegate);

	ChildBuilder.AddCustomRow(FText::GetEmpty())
		.NameContent()
		[
			SNew(STextBlock)
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.AutoWrapText(true)
				.Text(LOCTEXT("Class","Class"))
		]
		.ValueContent()
		[
			ClassView.ToSharedRef()
		];

	ChildBuilder.AddCustomRow(FText::GetEmpty())
		.NameContent()
		[
			SNew(STextBlock)
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.AutoWrapText(true)
				.Text(LOCTEXT("Property", "Property"))
		]
		.ValueContent()
		[
			SNew(SBox)
				.Padding(4,0)
				[
					SAssignNew(mPropertyComboBox, STextComboBox)
						.OptionsSource(&mPropertyList)
						.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						.InitiallySelectedItem(InitiallyProperty)
						.OnSelectionChanged(this, &FPropertyTypeCustomization_LevelStreamingPersistentProperty::OnProxyPropertyNameChanged)
				]
		];

	ChildBuilder.AddCustomRow(FText::GetEmpty())
		.NameContent()
		[
			SNew(STextBlock)
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.AutoWrapText(true)
				.Text(LOCTEXT("Public", "Public"))
				.ToolTipText(LOCTEXT("PublicToolTip", "If it is public, it will be allowed to be set by the ULevelStreamingPersistenceManager"))
		]
		.ValueContent()
		[
			PublicView.ToSharedRef()
		];
}

void FPropertyTypeCustomization_LevelStreamingPersistentProperty::OnProxyClassPropertyChanged()
{
	mPropertyList.Empty();
	for (TFieldIterator<FProperty> It(mPropertyProxy->Class.Get()); It; ++It) {
		mPropertyList.Add(MakeShared<FString>(It->GetName()));
	}
	if(mPropertyComboBox)
		mPropertyComboBox->RefreshOptions();
}

void FPropertyTypeCustomization_LevelStreamingPersistentProperty::OnProxyPublicPropertyChanged()
{
	mPropertyPtr->bIsPublic = mPropertyProxy->bPublic;
	mPropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	mPropertyHandle->NotifyFinishedChangingProperties();
	GetMutableDefault<ULevelStreamingPersistenceSettings>()->TryUpdateDefaultConfigFile();
}

void FPropertyTypeCustomization_LevelStreamingPersistentProperty::OnProxyPropertyNameChanged(TSharedPtr<FString> InText, ESelectInfo::Type InType)
{
	mPropertyProxy->PropertyName = *InText;
	if (FProperty* Property = FindFProperty<FProperty>(mPropertyProxy->Class.Get(), *mPropertyProxy->PropertyName)) {
		mPropertyPtr->Path = Property->GetPathName();
	}
	mPropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	mPropertyHandle->NotifyFinishedChangingProperties();
	GetMutableDefault<ULevelStreamingPersistenceSettings>()->TryUpdateDefaultConfigFile();
}

TSharedPtr<SWidget> ULevelStreamingPersistenceEditor::BuildWidget()
{
	static bool IsRegister = false;
	if (!IsRegister) {
		IsRegister = true;
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout(ULevelStreamingPersistenceSettings::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FDetailCustomization_LevelStreamingPersistenceEditorSettings::MakeInstance));
		PropertyModule.RegisterCustomPropertyTypeLayout(FLevelStreamingPersistentProperty::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FPropertyTypeCustomization_LevelStreamingPersistentProperty::MakeInstance));
	}
	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	//DetailsViewArgs.bAllowMultipleTopLevelObjects = true;
	DetailsViewArgs.bShowObjectLabel = false;
	DetailsViewArgs.bAllowSearch = true;
	DetailsViewArgs.bAllowFavoriteSystem = true;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::ENameAreaSettings::HideNameArea;
	DetailsViewArgs.ViewIdentifier = FName("BlueprintDefaults");
	auto DetailView = EditModule.CreateDetailView(DetailsViewArgs);
	DetailView->SetObject(GetMutableDefault<ULevelStreamingPersistenceSettings>());
	return DetailView;
}

#undef LOCTEXT_NAMESPACE
