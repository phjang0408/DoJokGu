#include "Jokgu/UI/JGSettingsPanelWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace
{
	const FLinearColor SelectedColor(1.0f, 0.75f, 0.2f);
	const FLinearColor IdleColor(1.0f, 1.0f, 1.0f);
}

#pragma region 초기화와 기본 레이아웃

void UJGSettingsPanelWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}

	if (MasterSlider)
	{
		MasterSlider->OnValueChanged.AddUniqueDynamic(this, &UJGSettingsPanelWidget::HandleMasterChanged);
		MasterSlider->OnMouseCaptureEnd.AddUniqueDynamic(this, &UJGSettingsPanelWidget::HandleSliderReleased);
	}

	if (EffectsSlider)
	{
		EffectsSlider->OnValueChanged.AddUniqueDynamic(this, &UJGSettingsPanelWidget::HandleEffectsChanged);
		EffectsSlider->OnMouseCaptureEnd.AddUniqueDynamic(this, &UJGSettingsPanelWidget::HandleSliderReleased);
	}

	if (MusicSlider)
	{
		MusicSlider->OnValueChanged.AddUniqueDynamic(this, &UJGSettingsPanelWidget::HandleMusicChanged);
		MusicSlider->OnMouseCaptureEnd.AddUniqueDynamic(this, &UJGSettingsPanelWidget::HandleSliderReleased);
	}

	if (LowButton)
	{
		LowButton->OnClicked.AddUniqueDynamic(this, &UJGSettingsPanelWidget::HandleLowClicked);
	}

	if (MediumButton)
	{
		MediumButton->OnClicked.AddUniqueDynamic(this, &UJGSettingsPanelWidget::HandleMediumClicked);
	}

	if (HighButton)
	{
		HighButton->OnClicked.AddUniqueDynamic(this, &UJGSettingsPanelWidget::HandleHighClicked);
	}

	if (CloseButton)
	{
		CloseButton->OnClicked.AddUniqueDynamic(this, &UJGSettingsPanelWidget::HandleCloseClicked);
	}

	SyncFromSettings();
}

void UJGSettingsPanelWidget::BuildDefaultLayout()
{
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	Panel->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.7f));
	Panel->SetPadding(FMargin(28.0f, 18.0f));
	WidgetTree->RootWidget = Panel;

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	Panel->SetContent(Column);

	auto MakeText = [this](const FString& Content, int32 FontSize, const FLinearColor& Color)
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = FontSize;
		Text->SetFont(Font);
		Text->SetText(FText::FromString(Content));
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Text;
	};

	auto AddRow = [Column](UWidget* Widget, float TopPadding)
	{
		if (UVerticalBoxSlot* RowSlot = Column->AddChildToVerticalBox(Widget))
		{
			RowSlot->SetHorizontalAlignment(HAlign_Center);
			RowSlot->SetPadding(FMargin(0.0f, TopPadding, 0.0f, 0.0f));
		}
	};

	AddRow(MakeText(TEXT("Settings"), 30, FLinearColor::White), 0.0f);

	// 이름 칸과 슬라이더 칸의 너비를 고정해서 세 줄이 맞춰 보이게 한다
	auto AddSliderRow = [&](const TCHAR* Label, const TCHAR* Name) -> USlider*
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

		USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		LabelBox->SetWidthOverride(160.0f);
		LabelBox->SetContent(MakeText(Label, 22, FLinearColor::White));
		Row->AddChildToHorizontalBox(LabelBox)->SetVerticalAlignment(VAlign_Center);

		USlider* Slider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), Name);
		Slider->SetMinValue(0.0f);
		Slider->SetMaxValue(1.0f);
		Slider->SetStepSize(0.05f);

		USizeBox* SliderBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		SliderBox->SetWidthOverride(320.0f);
		SliderBox->SetHeightOverride(44.0f);
		SliderBox->SetContent(Slider);
		Row->AddChildToHorizontalBox(SliderBox)->SetVerticalAlignment(VAlign_Center);

		AddRow(Row, 14.0f);
		return Slider;
	};

	MasterSlider = AddSliderRow(TEXT("Master"), TEXT("MasterSlider"));
	EffectsSlider = AddSliderRow(TEXT("Effects"), TEXT("EffectsSlider"));
	MusicSlider = AddSliderRow(TEXT("Music"), TEXT("MusicSlider"));

	AddRow(MakeText(TEXT("Graphics"), 22, FLinearColor::White), 18.0f);

	UHorizontalBox* QualityRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	auto AddQualityButton = [&](const TCHAR* Label, const TCHAR* Name)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		Button->AddChild(MakeText(FString::Printf(TEXT("  %s  "), Label), 22, FLinearColor::Black));
		QualityRow->AddChildToHorizontalBox(Button)->SetPadding(FMargin(8.0f, 0.0f));
		return Button;
	};

	LowButton = AddQualityButton(TEXT("Low"), TEXT("LowButton"));
	MediumButton = AddQualityButton(TEXT("Medium"), TEXT("MediumButton"));
	HighButton = AddQualityButton(TEXT("High"), TEXT("HighButton"));
	AddRow(QualityRow, 8.0f);

	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CloseButton"));
	CloseButton->AddChild(MakeText(TEXT("  Close  "), 24, FLinearColor::Black));
	AddRow(CloseButton, 20.0f);
}

void UJGSettingsPanelWidget::SyncFromSettings()
{
	const UJGSettingsSubsystem* Settings = UJGSettingsSubsystem::Get(this);
	if (!Settings)
	{
		return;
	}

	// SetValue는 OnValueChanged를 부르지 않으므로 저장이 일어나지 않는다
	if (MasterSlider)
	{
		MasterSlider->SetValue(Settings->GetVolume(EJGVolumeChannel::Master));
	}

	if (EffectsSlider)
	{
		EffectsSlider->SetValue(Settings->GetVolume(EJGVolumeChannel::Effects));
	}

	if (MusicSlider)
	{
		MusicSlider->SetValue(Settings->GetVolume(EJGVolumeChannel::Music));
	}

	HighlightQuality(Settings->GetGraphicsQuality());
}

void UJGSettingsPanelWidget::HighlightQuality(EJGGraphicsQuality Quality)
{
	const TPair<UButton*, EJGGraphicsQuality> Buttons[] = {
		{ LowButton, EJGGraphicsQuality::Low },
		{ MediumButton, EJGGraphicsQuality::Medium },
		{ HighButton, EJGGraphicsQuality::High },
	};

	for (const TPair<UButton*, EJGGraphicsQuality>& Entry : Buttons)
	{
		if (Entry.Key)
		{
			Entry.Key->SetBackgroundColor(Entry.Value == Quality ? SelectedColor : IdleColor);
		}
	}
}

#pragma endregion

#pragma region 위젯 이벤트

void UJGSettingsPanelWidget::ChangeVolume(EJGVolumeChannel Channel, float Value)
{
	if (UJGSettingsSubsystem* Settings = UJGSettingsSubsystem::Get(this))
	{
		Settings->SetVolume(Channel, Value, false);
	}
}

void UJGSettingsPanelWidget::PickQuality(EJGGraphicsQuality Quality)
{
	if (UJGSettingsSubsystem* Settings = UJGSettingsSubsystem::Get(this))
	{
		Settings->SetGraphicsQuality(Quality);
	}

	HighlightQuality(Quality);
}

void UJGSettingsPanelWidget::HandleMasterChanged(float Value)
{
	ChangeVolume(EJGVolumeChannel::Master, Value);
}

void UJGSettingsPanelWidget::HandleEffectsChanged(float Value)
{
	ChangeVolume(EJGVolumeChannel::Effects, Value);
}

void UJGSettingsPanelWidget::HandleMusicChanged(float Value)
{
	ChangeVolume(EJGVolumeChannel::Music, Value);
}

void UJGSettingsPanelWidget::HandleSliderReleased()
{
	if (UJGSettingsSubsystem* Settings = UJGSettingsSubsystem::Get(this))
	{
		Settings->SaveProfile();
	}
}

void UJGSettingsPanelWidget::HandleLowClicked()
{
	PickQuality(EJGGraphicsQuality::Low);
}

void UJGSettingsPanelWidget::HandleMediumClicked()
{
	PickQuality(EJGGraphicsQuality::Medium);
}

void UJGSettingsPanelWidget::HandleHighClicked()
{
	PickQuality(EJGGraphicsQuality::High);
}

void UJGSettingsPanelWidget::HandleCloseClicked()
{
	// 터치로 슬라이더를 끈 뒤 캡처 종료가 오지 않는 경우에도 값이 남도록 닫을 때 한 번 더 저장한다
	if (UJGSettingsSubsystem* Settings = UJGSettingsSubsystem::Get(this))
	{
		Settings->SaveProfile();
	}

	OnCloseRequested.Broadcast();
}

#pragma endregion
