#include "Jokgu/UI/JGLobbyHUDWidget.h"
#include "Jokgu/Online/JGSessionSubsystem.h"
#include "Jokgu/UI/JGLookDragWidget.h"
#include "Jokgu/UI/JGMatchPanelWidget.h"
#include "Jokgu/UI/JGSettingsPanelWidget.h"
#include "Jokgu/UI/JGVirtualJoystickWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
	constexpr float ReturnReasonSeconds = 6.0f;
}

#pragma region 초기화와 기본 레이아웃

void UJGLobbyHUDWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}

	if (ZoneButton)
	{
		ZoneButton->OnClicked.AddUniqueDynamic(this, &UJGLobbyHUDWidget::HandleZoneClicked);
		ZoneButton->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (PlayShortcut)
	{
		PlayShortcut->OnClicked.AddUniqueDynamic(this, &UJGLobbyHUDWidget::HandlePlayShortcut);
	}

	if (SettingsShortcut)
	{
		SettingsShortcut->OnClicked.AddUniqueDynamic(this, &UJGLobbyHUDWidget::HandleSettingsShortcut);
	}

	if (MatchPanel)
	{
		MatchPanel->OnCloseRequested.AddUObject(this, &UJGLobbyHUDWidget::ClosePanels);
	}

	if (SettingsPanel)
	{
		SettingsPanel->OnCloseRequested.AddUObject(this, &UJGLobbyHUDWidget::ClosePanels);
	}

	ClosePanels();
	SetTouchControlsVisible(bTouchControlsVisible);
	ShowReturnReason();
}

void UJGLobbyHUDWidget::BuildDefaultLayout()
{
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	// 화면 전체: 카메라 드래그. 먼저 추가해서 버튼과 조이스틱이 터치를 먼저 받는다.
	LookArea = WidgetTree->ConstructWidget<UJGLookDragWidget>(UJGLookDragWidget::StaticClass(), TEXT("LookArea"));
	if (UCanvasPanelSlot* LookSlot = RootCanvas->AddChildToCanvas(LookArea))
	{
		LookSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		LookSlot->SetOffsets(FMargin(0.0f));
	}

	// 왼쪽 아래: 이동 조이스틱
	Joystick = WidgetTree->ConstructWidget<UJGVirtualJoystickWidget>(UJGVirtualJoystickWidget::StaticClass(), TEXT("Joystick"));
	if (UCanvasPanelSlot* JoystickSlot = RootCanvas->AddChildToCanvas(Joystick))
	{
		JoystickSlot->SetAnchors(FAnchors(0.0f, 1.0f));
		JoystickSlot->SetAlignment(FVector2D(0.0f, 1.0f));
		JoystickSlot->SetPosition(FVector2D(60.0f, -60.0f));
		JoystickSlot->SetSize(FVector2D(260.0f, 260.0f));
	}

	auto MakeText = [this](const FString& Content, int32 FontSize, const FLinearColor& Color)
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = FontSize;
		Text->SetFont(Font);
		Text->SetText(FText::FromString(Content));
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetJustification(ETextJustify::Center);
		Text->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Text;
	};

	auto MakeButton = [&](const FString& Label, const TCHAR* Name, int32 FontSize, UTextBlock** OutLabel = nullptr)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		UTextBlock* Text = MakeText(Label, FontSize, FLinearColor::Black);
		Button->AddChild(Text);
		if (OutLabel)
		{
			*OutLabel = Text;
		}
		return Button;
	};

	// 가운데 아래: 구역 버튼. 구역에 들어갔을 때만 보인다.
	UTextBlock* Label = nullptr;
	ZoneButton = MakeButton(TEXT("  Play  "), TEXT("ZoneButton"), 30, &Label);
	ZoneLabel = Label;
	if (UCanvasPanelSlot* ZoneSlot = RootCanvas->AddChildToCanvas(ZoneButton))
	{
		ZoneSlot->SetAnchors(FAnchors(0.5f, 1.0f));
		ZoneSlot->SetAlignment(FVector2D(0.5f, 1.0f));
		ZoneSlot->SetPosition(FVector2D(0.0f, -80.0f));
		ZoneSlot->SetAutoSize(true);
	}

	// 오른쪽 위: 걸어가지 않아도 여는 바로가기
	UHorizontalBox* Shortcuts = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Shortcuts"));
	PlayShortcut = MakeButton(TEXT(" Play "), TEXT("PlayShortcut"), 22);
	SettingsShortcut = MakeButton(TEXT(" Settings "), TEXT("SettingsShortcut"), 22);
	Shortcuts->AddChildToHorizontalBox(PlayShortcut)->SetPadding(FMargin(6.0f, 0.0f));
	Shortcuts->AddChildToHorizontalBox(SettingsShortcut)->SetPadding(FMargin(6.0f, 0.0f));
	if (UCanvasPanelSlot* ShortcutSlot = RootCanvas->AddChildToCanvas(Shortcuts))
	{
		ShortcutSlot->SetAnchors(FAnchors(1.0f, 0.0f));
		ShortcutSlot->SetAlignment(FVector2D(1.0f, 0.0f));
		ShortcutSlot->SetPosition(FVector2D(-30.0f, 30.0f));
		ShortcutSlot->SetAutoSize(true);
	}

	// 위 가운데: 돌아온 이유 같은 한 줄 알림
	MessageText = MakeText(TEXT(""), 24, FLinearColor(1.0f, 0.85f, 0.5f));
	MessageText->SetShadowOffset(FVector2D(2.0f, 2.0f));
	MessageText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
	if (UCanvasPanelSlot* MessageSlot = RootCanvas->AddChildToCanvas(MessageText))
	{
		MessageSlot->SetAnchors(FAnchors(0.5f, 0.0f));
		MessageSlot->SetAlignment(FVector2D(0.5f, 0.0f));
		MessageSlot->SetPosition(FVector2D(0.0f, 30.0f));
		MessageSlot->SetAutoSize(true);
	}

	// 가운데: 패널 두 개. 한 번에 하나만 보인다.
	auto AddCentered = [RootCanvas](UWidget* Widget)
	{
		if (UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(Widget))
		{
			PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			PanelSlot->SetAutoSize(true);
		}
	};

	MatchPanel = WidgetTree->ConstructWidget<UJGMatchPanelWidget>(UJGMatchPanelWidget::StaticClass(), TEXT("MatchPanel"));
	AddCentered(MatchPanel);

	SettingsPanel = WidgetTree->ConstructWidget<UJGSettingsPanelWidget>(UJGSettingsPanelWidget::StaticClass(), TEXT("SettingsPanel"));
	AddCentered(SettingsPanel);
}

#pragma endregion

#pragma region 구역과 패널

void UJGLobbyHUDWidget::SetTouchControlsVisible(bool bVisible)
{
	bTouchControlsVisible = bVisible;

	if (Joystick)
	{
		Joystick->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UJGLobbyHUDWidget::SetActiveZone(const AJGInteractZone* Zone)
{
	if (!ZoneButton)
	{
		return;
	}

	if (!Zone)
	{
		ZoneButton->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	ActiveZoneKind = Zone->GetKind();
	ZoneButton->SetVisibility(ESlateVisibility::Visible);

	if (ZoneLabel)
	{
		ZoneLabel->SetText(FText::FromString(FString::Printf(TEXT("  %s  "), *Zone->GetDisplayName().ToString())));
	}
}

void UJGLobbyHUDWidget::OpenPanel(EJGZoneKind Kind)
{
	ClosePanels();

	if (Kind == EJGZoneKind::Match && MatchPanel)
	{
		MatchPanel->SetVisibility(ESlateVisibility::Visible);
		MatchPanel->Refresh();
	}
	else if (Kind == EJGZoneKind::Settings && SettingsPanel)
	{
		SettingsPanel->SetVisibility(ESlateVisibility::Visible);
		SettingsPanel->SyncFromSettings();
	}
}

void UJGLobbyHUDWidget::ClosePanels()
{
	if (MatchPanel)
	{
		MatchPanel->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (SettingsPanel)
	{
		SettingsPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UJGLobbyHUDWidget::HandleZoneClicked()
{
	OpenPanel(ActiveZoneKind);
}

void UJGLobbyHUDWidget::HandlePlayShortcut()
{
	OpenPanel(EJGZoneKind::Match);
}

void UJGLobbyHUDWidget::HandleSettingsShortcut()
{
	OpenPanel(EJGZoneKind::Settings);
}

#pragma endregion

#pragma region 알림

void UJGLobbyHUDWidget::ShowReturnReason()
{
	UGameInstance* GameInstance = GetGameInstance();
	UJGSessionSubsystem* Sessions = GameInstance ? GameInstance->GetSubsystem<UJGSessionSubsystem>() : nullptr;

	if (!MessageText || !Sessions || Sessions->GetLastFailureReason().IsEmpty())
	{
		return;
	}

	MessageText->SetText(FText::FromString(Sessions->GetLastFailureReason()));
	Sessions->ClearLastFailureReason();

	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(MessageTimer, this, &UJGLobbyHUDWidget::HideMessage, ReturnReasonSeconds, false);
	}
}

void UJGLobbyHUDWidget::HideMessage()
{
	if (MessageText)
	{
		MessageText->SetText(FText::GetEmpty());
	}
}

#pragma endregion
