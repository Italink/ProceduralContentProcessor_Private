#include "HLODPreview.h"
#include "Widgets/SCanvas.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/STextComboBox.h"
#include "WorldPartition/ActorDescContainer.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/HLOD/HLODActor.h"
#include "Selection.h"
#include "LevelEditorViewport.h"
#include "WorldPartition/HLOD/HLODSourceActorsFromCell.h"

#define LOCTEXT_NAMESPACE "ProceduralContentProcessor"

class SHLODOutliner : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHLODOutliner) {}
	SLATE_END_ARGS()
public:
	void Construct(const FArguments& InArgs);
	void SetWorld(UWorld* InWorld);

	struct FActorDescInfo {
		bool bIsPreview = false;
		int CellX;
		int CellY;
		TSharedPtr<FWorldPartitionActorDesc> ActorDesc;
	};
	struct FHLODLevelInfo {
		TArray<FActorDescInfo> HLods;
		FBox LevelBound;
	};
private:
	void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	TSharedRef<SWidget> OnGetBlockMenu(TSharedPtr<FWorldPartitionActorDesc> InActorDesc);
	FVector2D OnGetBlockPosition(TSharedPtr<FWorldPartitionActorDesc> InActorDesc, FBox InLevelBound);
	bool GetObserverView(FVector& Location, FRotator& Rotation) const;
	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FWorldPartitionActorDesc> InInfo, const TSharedRef<STableViewBase>& OwnerTable);
	FVector2D OnGetBlockSize(TSharedPtr<FWorldPartitionActorDesc> InActorDesc, FBox InLevelBound);
	void OnGridNameChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo);
	void OnHLodNameChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo);

	FReply OnBlockClicked(FActorDescInfo& Info);

	void OnShowHLODActor(TSharedPtr<FWorldPartitionActorDesc> InActorDesc);
	void OnHideHLODActor(TSharedPtr<FWorldPartitionActorDesc> InActorDesc);
	void OnViewHLODMesh(TSharedPtr<FWorldPartitionActorDesc> InActorDesc);
	void OnSelectHLODSourceActor(TSharedPtr<FWorldPartitionActorDesc> InActorDesc);
protected:
	int mCurrentLevel;
	TMap<FString, TMap<FString, TMap<int, FHLODLevelInfo>>> mLevelActorDescInfoMap;
	TArray<TSharedPtr<FString>> mGridNames;
	TSharedPtr<STextComboBox> mGridNameComboBox;
	TArray<TSharedPtr<FString>> mHLODNames;
	TSharedPtr<STextComboBox> mHLODNameComboBox;
	TSharedPtr<SSlider> mLevelSilder;
	TSharedPtr<SCanvas> mCanvas;
	bool bNeedUpdateCanvas = false;
};

void SHLODOutliner::Construct(const FArguments& InArgs)
{
	ChildSlot
		[
			SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Top)
				.Padding(5)
				.AutoHeight()
				[
					SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
								.Text(FText::FromString(TEXT("Grid:")))
						]
						+ SHorizontalBox::Slot()[
							SAssignNew(mGridNameComboBox, STextComboBox)
								.OptionsSource(&mGridNames)
								.OnSelectionChanged(this, &SHLODOutliner::OnGridNameChanged)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(10, 0, 0, 0)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
								.Text(FText::FromString(TEXT("HLOD:")))
						]
						+ SHorizontalBox::Slot()[
							SAssignNew(mHLODNameComboBox, STextComboBox)
								.OptionsSource(&mHLODNames)
								.OnSelectionChanged(this, &SHLODOutliner::OnHLodNameChanged)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(10, 0, 0, 0)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
								.Text_Lambda([this]() {return FText::FromString(FString::Printf(TEXT("Level: %d"), mCurrentLevel)); })
						]
						+ SHorizontalBox::Slot()[
							SAssignNew(mLevelSilder, SSlider)
								.StepSize(1.0f)
								.Value_Lambda([this]() {return mCurrentLevel; })
								.OnValueChanged_Lambda([this](float var) {
								mCurrentLevel = var;
								bNeedUpdateCanvas = true;
									})
						]
				]
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				[
					SNew(SBorder)
						.HAlign(HAlign_Fill)
						.VAlign(VAlign_Fill)
						[
							SAssignNew(mCanvas, SCanvas)
						]
				]
		];
}

void SHLODOutliner::SetWorld(UWorld* InWorld)
{
	mLevelActorDescInfoMap.Empty();
	if (InWorld == nullptr || InWorld->GetWorldPartition() == nullptr)
		return;
	UWorldPartition* WorldPartition = InWorld->GetWorldPartition();
	WorldPartition->ForEachActorDescContainer([&](UActorDescContainer* Containter) {
		for (UActorDescContainer::TIterator<> Iterator(Containter); Iterator; ++Iterator) {
			UClass* ActorClass = Iterator->GetActorNativeClass();
			if (ActorClass == AWorldPartitionHLOD::StaticClass()) {
				FWorldPartitionActorDesc Desc = **Iterator;
				FString ActorName = Iterator->GetActorLabelOrName().ToString();
				FRegexMatcher Matcher(FRegexPattern(TEXT("(.+)/(.+)_L(.+)_X(.+)_Y(.+)"), ERegexPatternFlags::CaseInsensitive), ActorName);
				if (Matcher.FindNext()) {
					FString HLODName = Matcher.GetCaptureGroup(1);
					FString GridName = Matcher.GetCaptureGroup(2);
					int LevelIndex = FCString::Atoi(*Matcher.GetCaptureGroup(3));
					int CellX = FCString::Atoi(*Matcher.GetCaptureGroup(4));
					int CellY = FCString::Atoi(*Matcher.GetCaptureGroup(5));
					FActorDescInfo NewInfo;
					NewInfo.CellX = CellX;
					NewInfo.CellY = CellY;
					NewInfo.ActorDesc = MakeShared<FWorldPartitionActorDesc>(Desc);
					FHLODLevelInfo& LevelInfo = mLevelActorDescInfoMap.FindOrAdd(GridName).FindOrAdd(HLODName).FindOrAdd(LevelIndex);
					LevelInfo.HLods.Add(NewInfo);
					LevelInfo.LevelBound += Iterator->GetEditorBounds();;
					bool bExisted = false;
					for (auto Name : mGridNames) {
						if (*Name == GridName) {
							bExisted = true;
							break;
						}
					}
					if (!bExisted)
						mGridNames.Add(MakeShared<FString>(GridName));
				}
			}
		}
		});
	if (!mGridNames.IsEmpty()) {
		mGridNameComboBox->SetSelectedItem(mGridNames[0]);
	}
}

TSharedRef<ITableRow> SHLODOutliner::OnGenerateRow(TSharedPtr<FWorldPartitionActorDesc> InInfo, const TSharedRef<STableViewBase>& InOwnerTable) {
	return SNew(STableRow<TSharedRef<FWorldPartitionActorDesc>>, InOwnerTable)
		[
			SNew(STextBlock)
				.Text(FText::FromName(InInfo->GetActorLabel()))
		];
}

bool SHLODOutliner::GetObserverView(FVector& Location, FRotator& Rotation) const
{
	// We are in the SIE
	if (GEditor->bIsSimulatingInEditor && GCurrentLevelEditingViewportClient->IsSimulateInEditorViewport())
	{
		Rotation = GCurrentLevelEditingViewportClient->GetViewRotation();
		Location = GCurrentLevelEditingViewportClient->GetViewLocation();
		return true;
	}

	// We are in the editor world
	if (!GEditor->PlayWorld)
	{
		for (const FLevelEditorViewportClient* ViewportClient : GEditor->GetLevelViewportClients())
		{
			if (ViewportClient && ViewportClient->IsPerspective())
			{
				Rotation = ViewportClient->GetViewRotation();
				Location = ViewportClient->GetViewLocation();
				return true;
			}
		}
	}
	return false;
}

void SHLODOutliner::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (bNeedUpdateCanvas) {
		mCanvas->ClearChildren();
		TSharedPtr<FString> CurrentGridName = mGridNameComboBox->GetSelectedItem();
		TSharedPtr<FString> CurrentHLodName = mHLODNameComboBox->GetSelectedItem();
		int CurrentLevelIndex = mLevelSilder->GetValue();
		if (CurrentGridName && CurrentHLodName) {
			auto& LevelInfo = mLevelActorDescInfoMap[*CurrentGridName][*CurrentHLodName][CurrentLevelIndex];
			for (FActorDescInfo& ActorDescInfo : LevelInfo.HLods) {
				FText CellName = FText::FromString(FString::Printf(TEXT("%3d,%3d"), ActorDescInfo.CellX, ActorDescInfo.CellY));
				mCanvas->AddSlot()
					.Position(TAttribute<FVector2D>::Create(TAttribute<FVector2D>::FGetter::CreateSP(this, &SHLODOutliner::OnGetBlockPosition, ActorDescInfo.ActorDesc, LevelInfo.LevelBound)))
					.Size(TAttribute<FVector2D>::Create(TAttribute<FVector2D>::FGetter::CreateSP(this, &SHLODOutliner::OnGetBlockSize, ActorDescInfo.ActorDesc, LevelInfo.LevelBound)))
					[
						SNew(SButton)
							.VAlign(VAlign_Center)
							.HAlign(HAlign_Center)
							.Text(CellName)
							.ToolTipText(CellName)
							.ButtonColorAndOpacity_Lambda([&ActorDescInfo]() { return ActorDescInfo.bIsPreview ? FLinearColor(0, 1, 0, 1) : FLinearColor(0, 0, 0, 1); })
							.ButtonStyle(FAppStyle::Get(), "Button")
							//.ButtonStyle(FAppStyle::Get(), "FlatButton.Success")
							.OnClicked_Lambda([this, &ActorDescInfo]() {
							return OnBlockClicked(ActorDescInfo);
								})
					];
			}
		}
		bNeedUpdateCanvas = false;
	}
}

int32 SHLODOutliner::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int NewLayerId = SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	TSharedPtr<FString> CurrentGridName = mGridNameComboBox->GetSelectedItem();
	TSharedPtr<FString> CurrentHLodName = mHLODNameComboBox->GetSelectedItem();
	int CurrentLevelIndex = mLevelSilder->GetValue();
	if (CurrentGridName && CurrentHLodName) {
		auto& LevelInfo = mLevelActorDescInfoMap[*CurrentGridName][*CurrentHLodName][CurrentLevelIndex];
		const FVector2D ShadowSize(2, 2);
		const FSlateBrush* CameraImage = FAppStyle::GetBrush(TEXT("WorldPartition.SimulationViewPosition"));
		FVector ObserverPosition;
		FRotator ObserverRotation;
		if (GetObserverView(ObserverPosition, ObserverRotation))
		{
			FBox LevelBound = LevelInfo.LevelBound;
			FVector2D Position(
				(ObserverPosition.X - LevelBound.Min.X) / (LevelBound.Max.X - LevelBound.Min.X),
				(ObserverPosition.Y - LevelBound.Min.Y) / (LevelBound.Max.Y - LevelBound.Min.Y)
			);
			const FVector2D LocalLocation = Position * mCanvas->GetPersistentState().AllottedGeometry.Size;
			const FPaintGeometry PaintGeometryShadow = AllottedGeometry.ToPaintGeometry(
				CameraImage->ImageSize + ShadowSize,
				FSlateLayoutTransform(LocalLocation - (CameraImage->ImageSize + ShadowSize) * 0.5f)
			);

			FSlateDrawElement::MakeRotatedBox(
				OutDrawElements,
				++NewLayerId,
				PaintGeometryShadow,
				CameraImage,
				ESlateDrawEffect::None,
				FMath::DegreesToRadians(ObserverRotation.Yaw),
				(CameraImage->ImageSize + ShadowSize) * 0.5f,
				FSlateDrawElement::RelativeToElement,
				FLinearColor::Black
			);

			const FPaintGeometry PaintGeometry = AllottedGeometry.ToPaintGeometry(
				CameraImage->ImageSize,
				FSlateLayoutTransform(LocalLocation - CameraImage->ImageSize * 0.5f)
			);

			FSlateDrawElement::MakeRotatedBox(
				OutDrawElements,
				++NewLayerId,
				PaintGeometry,
				CameraImage,
				ESlateDrawEffect::None,
				FMath::DegreesToRadians(ObserverRotation.Yaw),
				CameraImage->ImageSize * 0.5f,
				FSlateDrawElement::RelativeToElement,
				FLinearColor::White
			);
		}
	}
	return NewLayerId;
}

TSharedRef<SWidget> SHLODOutliner::OnGetBlockMenu(TSharedPtr<FWorldPartitionActorDesc> InActorDesc) {
	FMenuBuilder MenuBuilder(true, nullptr);
	AActor* Actor = InActorDesc->GetActor();
	UWorld* World = GEditor->GetEditorWorldContext(true).World();
	UWorldPartition* WorldPartition = World->GetWorldPartition();
	if (!InActorDesc->IsLoaded()) {
		WorldPartition->PinActors({ InActorDesc->GetGuid() });
		Actor = InActorDesc->GetActor();
		Actor->SetIsTemporarilyHiddenInEditor(true);
	}
	if (Actor) {
		if (Actor->IsTemporarilyHiddenInEditor()) {
			MenuBuilder.AddMenuEntry(
				LOCTEXT("ShowHLODActor", "Show HLOD Actor"),
				LOCTEXT("ShowHLODActor", "Show HLOD Actor"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(this, &SHLODOutliner::OnShowHLODActor, InActorDesc)
				),
				NAME_None,
				EUserInterfaceActionType::Button);
		}
		else {
			MenuBuilder.AddMenuEntry(
				LOCTEXT("HideHLODActor", "Hide HLOD Actor"),
				LOCTEXT("HideHLODActor", "Hide HLOD Actor"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(this, &SHLODOutliner::OnHideHLODActor, InActorDesc)
				),
				NAME_None,
				EUserInterfaceActionType::Button);
		}
		MenuBuilder.AddMenuEntry(
			LOCTEXT("ViewHLODMesh", "View HLOD Mesh"),
			LOCTEXT("ViewHLODMesh", "View HLOD Mesh"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &SHLODOutliner::OnViewHLODMesh, InActorDesc)
			),
			NAME_None,
			EUserInterfaceActionType::Button);
		MenuBuilder.AddMenuEntry(
			LOCTEXT("SelectActors", "Select Actors"),
			LOCTEXT("SelectActors", "Select Actors"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &SHLODOutliner::OnSelectHLODSourceActor, InActorDesc)
			),
			NAME_None,
			EUserInterfaceActionType::Button);
	}
	return MenuBuilder.MakeWidget();
}

FVector2D SHLODOutliner::OnGetBlockPosition(TSharedPtr<FWorldPartitionActorDesc> InActorDesc, FBox InLevelBound)
{
	FBox Bound = InActorDesc->GetEditorBounds();
	FVector2D Position(
		(Bound.Min.X - InLevelBound.Min.X) / (InLevelBound.Max.X - InLevelBound.Min.X),
		(Bound.Min.Y - InLevelBound.Min.Y) / (InLevelBound.Max.Y - InLevelBound.Min.Y)
	);
	return Position * mCanvas->GetPersistentState().AllottedGeometry.Size;
}

FVector2D SHLODOutliner::OnGetBlockSize(TSharedPtr<FWorldPartitionActorDesc> InActorDesc, FBox InLevelBound)
{
	FBox Bound = InActorDesc->GetEditorBounds();
	FVector2D Size(
		(Bound.Max.X - Bound.Min.X) / (InLevelBound.Max.X - InLevelBound.Min.X),
		(Bound.Max.Y - Bound.Min.Y) / (InLevelBound.Max.Y - InLevelBound.Min.Y)
	);
	return Size * mCanvas->GetPersistentState().AllottedGeometry.Size;
}

void SHLODOutliner::OnGridNameChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo)
{
	if (TMap<FString, TMap<int, FHLODLevelInfo>>* GridInfo = mLevelActorDescInfoMap.Find(*Selection)) {
		mHLODNames.Empty();
		for (auto HLodName : *GridInfo) {
			mHLODNames.Add(MakeShared<FString>(HLodName.Key));
		}
	}
	mLevelSilder->SetValue(0);
	mHLODNameComboBox->RefreshOptions();
	if (!mHLODNames.IsEmpty()) {
		mHLODNameComboBox->SetSelectedItem(mHLODNames[0]);
	}
	bNeedUpdateCanvas = true;
}

void SHLODOutliner::OnHLodNameChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo)
{
	TSharedPtr<FString> CurrentGridName = mGridNameComboBox->GetSelectedItem();
	if (CurrentGridName) {
		if (TMap<int, FHLODLevelInfo>* HLODInfo = mLevelActorDescInfoMap[*CurrentGridName].Find(*Selection)) {
			int Min = 999, Max = 0;
			for (auto LevelInfo : *HLODInfo) {
				Min = FMath::Min(Min, LevelInfo.Key);
				Max = FMath::Max(Max, LevelInfo.Key);
			}
			mLevelSilder->SetMinAndMaxValues(Min, Max);
			bNeedUpdateCanvas = true;
		}
	}
}

FReply SHLODOutliner::OnBlockClicked(FActorDescInfo& Info)
{
	auto ActorDesc = Info.ActorDesc;
	UWorld* World = GEditor->GetEditorWorldContext(true).World();
	UWorldPartition* WorldPartition = World->GetWorldPartition();
	AActor* Actor = nullptr;
	if (!ActorDesc->IsLoaded()) {
		WorldPartition->PinActors({ ActorDesc->GetGuid() });
		Actor = ActorDesc->Load();
		if (!Actor)
			return FReply::Handled();
		Actor->SetIsTemporarilyHiddenInEditor(true);
	}
	else {
		Actor = ActorDesc->GetActor();
		if (!Actor)
			return FReply::Handled();
	}
	AWorldPartitionHLOD* HLODActor = Cast<AWorldPartitionHLOD>(Actor);
	UWorldPartitionHLODSourceActorsFromCell* SourceActors = Cast<UWorldPartitionHLODSourceActorsFromCell>(HLODActor->GetSourceActors());
	if (SourceActors == nullptr)
		return FReply::Handled();
	if (Actor->IsTemporarilyHiddenInEditor()) {
		GEditor->GetSelectedActors()->Modify();
		GEditor->GetSelectedActors()->BeginBatchSelectOperation();
		GEditor->SelectNone(false, true, true);
		GEditor->SelectActor(Actor, true, false, true);
		FBox Bound = ActorDesc->GetEditorBounds();
		//GEditor->MoveViewportCamerasToBox(Bound, true);
		DrawDebugBox(World, Bound.GetCenter(), Bound.GetExtent(), FColor(255, 0, 0), false, 2, 0, 20);
		GEditor->GetSelectedActors()->EndBatchSelectOperation(/*bNotify*/false);
		GEditor->NoteSelectionChange();
		Info.bIsPreview = true;
		Actor->SetIsTemporarilyHiddenInEditor(false);
		for (auto SourceActor : SourceActors->GetActors()) {
			if (FWorldPartitionActorDesc* SourceActorDesc = WorldPartition->GetActorDescContainer()->GetActorDesc(SourceActor.ActorInstanceGuid)) {
				AActor* Source = SourceActorDesc->GetActor();
				if (Source) {
					Source->SetIsTemporarilyHiddenInEditor(true);
				}
			}
		}
	}
	else {
		Info.bIsPreview = false;
		Actor->SetIsTemporarilyHiddenInEditor(true);
		GEditor->GetSelectedActors()->Modify();
		GEditor->GetSelectedActors()->BeginBatchSelectOperation();
		GEditor->SelectNone(false, true, true);
		for (auto SourceActor : SourceActors->GetActors()) {
			if (FWorldPartitionActorDesc* SourceActorDesc = WorldPartition->GetActorDescContainer()->GetActorDesc(SourceActor.ActorInstanceGuid)) {
				if (!SourceActorDesc->IsLoaded()) {
					WorldPartition->PinActors({ SourceActorDesc->GetGuid() });
				}
				AActor* Source = SourceActorDesc->Load();
				if (Source) {
					Source->SetIsTemporarilyHiddenInEditor(false);
					GEditor->SelectActor(Source, true, false, true);
				}
			}
		}
		GEditor->GetSelectedActors()->EndBatchSelectOperation(/*bNotify*/false);
		GEditor->NoteSelectionChange();
	}
	return FReply::Handled();
}

void SHLODOutliner::OnShowHLODActor(TSharedPtr<FWorldPartitionActorDesc> InActorDesc)
{
	AActor* Actor = InActorDesc->GetActor();
	UWorld* World = Actor->GetWorld();
	GEditor->GetSelectedActors()->Modify();
	GEditor->GetSelectedActors()->BeginBatchSelectOperation();
	GEditor->SelectNone(false, true, true);
	GEditor->SelectActor(Actor, true, false, true);
	FBox Bound = InActorDesc->GetEditorBounds();
	GEditor->MoveViewportCamerasToBox(Bound, true);
	DrawDebugBox(World, Bound.GetCenter(), Bound.GetExtent(), FColor(255, 0, 0), false, 2, 0, 4);
	GEditor->GetSelectedActors()->EndBatchSelectOperation(/*bNotify*/false);
	GEditor->NoteSelectionChange();
	Actor->SetIsTemporarilyHiddenInEditor(false);
}

void SHLODOutliner::OnHideHLODActor(TSharedPtr<FWorldPartitionActorDesc> InActorDesc)
{
	AActor* Actor = InActorDesc->GetActor();;
	Actor->SetIsTemporarilyHiddenInEditor(true);
}

void SHLODOutliner::OnViewHLODMesh(TSharedPtr<FWorldPartitionActorDesc> InActorDesc)
{
	AWorldPartitionHLOD* Actor = Cast<AWorldPartitionHLOD>(InActorDesc->GetActor());
	if (Actor) {
		TArray<UStaticMeshComponent*> StaticMeshComps;
		Actor->GetComponents<UStaticMeshComponent>(StaticMeshComps);
		for (auto StaticMeshComp : StaticMeshComps) {
			UStaticMesh* StaticMesh = StaticMeshComp->GetStaticMesh();
			if (StaticMesh) {
				GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(StaticMesh);
			}
		}
	}
}

void SHLODOutliner::OnSelectHLODSourceActor(TSharedPtr<FWorldPartitionActorDesc> InActorDesc)
{
	AWorldPartitionHLOD* Actor = Cast<AWorldPartitionHLOD>(InActorDesc->GetActor());
	if (Actor) {
		//Actor->GetSourceActors()->
	}
}

TSharedPtr<SWidget> UHLODPreview::BuildWidget()
{
	return SAssignNew(HLODOutliner, SHLODOutliner);
}
