#include "ReprojectionCalibrationEditorToolkit.h"

#define LOCTEXT_NAMESPACE "FCompositionUtilsEditorModule"


class SDynamicBrushTest : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDynamicBrushTest) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		ChildSlot
			[
				SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Top)
					[
						SNew(SBorder)
							[
								SNew(SBox)
									.WidthOverride(128)
									.HeightOverride(128)
									[
										SNew(SImage)
											.Image(this, &SDynamicBrushTest::GetImage)
									]
							]
					]
				+ SVerticalBox::Slot()
					.FillHeight(.2f)
					.HAlign(HAlign_Left)
					[
						SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(SEditableTextBox)
									.Text(this, &SDynamicBrushTest::GetFilenameText)
									.HintText(LOCTEXT("DynamicBrushTestLabel", "Type in full path to an image (png)"))
									.OnTextCommitted(this, &SDynamicBrushTest::LoadImage)

							]
						+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(2.0f)
							.VAlign(VAlign_Center)
							[
								SNew(SButton)
									.ContentPadding(1.0f)
									.Text(LOCTEXT("ResetLabel", "Reset"))
									.OnClicked(this, &SDynamicBrushTest::Reset)
							]
					]
			];
	}

	~SDynamicBrushTest()
	{
		Reset();
	}
private:
	const FSlateBrush* GetImage() const
	{
		return DynamicBrush.IsValid() ? DynamicBrush.Get() : FCoreStyle::Get().GetBrush("Checkerboard");
	}


	void LoadImage(const FText& Text, ETextCommit::Type CommitType)
	{
		FilenameText = Text;
		FString Filename = Text.ToString();
		// Note Slate will append the extension automatically so remove the extension
		FName BrushName(*FPaths::GetBaseFilename(Filename, false));

		DynamicBrush = MakeShareable(new FSlateDynamicImageBrush(BrushName, FVector2D(128, 128)));
	}

	FReply Reset()
	{
		FilenameText = FText::GetEmpty();
		DynamicBrush.Reset();
		return FReply::Handled();
	}

	FText GetFilenameText() const
	{
		return FilenameText;
	}
private:
	TSharedPtr<FSlateDynamicImageBrush> DynamicBrush;
	FText FilenameText;
};


void FReprojectionCalibrationEditorToolkit::InitEditor(const TArray<UObject*>& InObjects)
{
	ReprojectionCalibrationAsset = Cast<UReprojectionCalibration>(InObjects[0]);

	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("ReprojectionCalibrationEditorLayout")
	->AddArea(
		FTabManager::NewPrimaryArea()->SetOrientation(Orient_Horizontal)
		->Split(
			FTabManager::NewStack()
			->SetSizeCoefficient(0.8f)
			->AddTab("DynamicBrushTestTab", ETabState::OpenedTab)
			->SetHideTabWell(true)
		)
		->Split(
			FTabManager::NewStack()
			->SetSizeCoefficient(0.2f)
			->AddTab("ReprojectionCalibrationDetailsTab", ETabState::OpenedTab)
		)
	);

	FAssetEditorToolkit::InitAssetEditor(
		EToolkitMode::Standalone,
		{},
		"ReprojectionCalibrationEditor",
		Layout, 
		true,
		true,
		InObjects);
}

void FReprojectionCalibrationEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(INVTEXT("Reprojection Calibrator"));

	InTabManager->RegisterTabSpawner("DynamicBrushTestTab", FOnSpawnTab::CreateLambda([=](const FSpawnTabArgs&)
	{
		return SNew(SDockTab)
			[
				SNew(SDynamicBrushTest)
			];
	}))
	.SetDisplayName(INVTEXT("Dynamic Brush Test"))
	.SetGroup((WorkspaceMenuCategory.ToSharedRef()));

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	TSharedRef<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsView->SetObjects(TArray<UObject*>{ ReprojectionCalibrationAsset });
	InTabManager->RegisterTabSpawner("ReprojectionCalibrationDetailsTab", FOnSpawnTab::CreateLambda([=](const FSpawnTabArgs&)
	{
		return SNew(SDockTab)
			[
				DetailsView
			];
	}))
	.SetDisplayName(INVTEXT("Details"))
	.SetGroup(WorkspaceMenuCategory.ToSharedRef());
}

void FReprojectionCalibrationEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
	InTabManager->UnregisterTabSpawner("DynamicBrushTestTab");
	InTabManager->UnregisterTabSpawner("ReprojectionCalibrationDetailsTab");
}

#undef LOCTEXT_NAMESPACE