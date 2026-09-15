#include "Jokgu/UI/JGMatchHUDWidget.h"
#include "Jokgu/UI/JGAttackTouchWidget.h"
#include "Jokgu/UI/JGVirtualJoystickWidget.h"
#include "Jokgu/Core/JGGameStateBase.h"
#include "Jokgu/Core/JGPlayerState.h"
#include "Jokgu/Player/JGPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"

void UJGMatchHUDWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}

	if (RematchButton)
	{
		RematchButton->OnClicked.AddDynamic(this, &UJGMatchHUDWidget::HandleRematchClicked);
		RematchButton->SetVisibility(ESlateVisibility::Collapsed);
	}

	SetTouchControlsVisible(bTouchControlsVisible);
}

void UJGMatchHUDWidget::BuildDefaultLayout()
{
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	// right half: attack gestures (added first so text is drawn above it)
	AttackZone = WidgetTree->ConstructWidget<UJGAttackTouchWidget>(UJGAttackTouchWidget::StaticClass(), TEXT("AttackZone"));
	if (UCanvasPanelSlot* ZoneSlot = RootCanvas->AddChildToCanvas(AttackZone))
	{
		ZoneSlot->SetAnchors(FAnchors(0.5f, 0.0f, 1.0f, 1.0f));
		ZoneSlot->SetOffsets(FMargin(0.0f));
	}

	// bottom left: movement joystick
	Joystick = WidgetTree->ConstructWidget<UJGVirtualJoystickWidget>(UJGVirtualJoystickWidget::StaticClass(), TEXT("Joystick"));
	if (UCanvasPanelSlot* JoystickSlot = RootCanvas->AddChildToCanvas(Joystick))
	{
		JoystickSlot->SetAnchors(FAnchors(0.0f, 1.0f));
		JoystickSlot->SetAlignment(FVector2D(0.0f, 1.0f));
		JoystickSlot->SetPosition(FVector2D(60.0f, -60.0f));
		JoystickSlot->SetSize(FVector2D(260.0f, 260.0f));
	}

	auto MakeText = [this, RootCanvas](const TCHAR* Name, int32 FontSize, const FAnchors& Anchors, const FVector2D& Position)
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = FontSize;
		Text->SetFont(Font);
		Text->SetJustification(ETextJustify::Center);
		Text->SetShadowOffset(FVector2D(2.0f, 2.0f));
		Text->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
		Text->SetVisibility(ESlateVisibility::HitTestInvisible);

		if (UCanvasPanelSlot* TextSlot = RootCanvas->AddChildToCanvas(Text))
		{
			TextSlot->SetAnchors(Anchors);
			TextSlot->SetAlignment(FVector2D(0.5f, 0.0f));
			TextSlot->SetPosition(Position);
			TextSlot->SetAutoSize(true);
		}

		return Text;
	};

	ScoreText = MakeText(TEXT("ScoreText"), 40, FAnchors(0.5f, 0.0f), FVector2D(0.0f, 20.0f));
	PhaseText = MakeText(TEXT("PhaseText"), 24, FAnchors(0.5f, 0.0f), FVector2D(0.0f, 80.0f));
	MessageText = MakeText(TEXT("MessageText"), 36, FAnchors(0.5f, 0.35f), FVector2D::ZeroVector);

	RematchButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("RematchButton"));
	UTextBlock* ButtonLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RematchLabel"));
	ButtonLabel->SetText(FText::FromString(TEXT("  Rematch  ")));
	ButtonLabel->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
	RematchButton->AddChild(ButtonLabel);

	if (UCanvasPanelSlot* ButtonSlot = RootCanvas->AddChildToCanvas(RematchButton))
	{
		ButtonSlot->SetAnchors(FAnchors(0.5f, 0.55f));
		ButtonSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		ButtonSlot->SetAutoSize(true);
	}
}

void UJGMatchHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	TryBindGameState();

	if (const AJGGameStateBase* GameState = BoundGameState.Get())
	{
		UpdatePhaseText(*GameState);
	}
}

void UJGMatchHUDWidget::NativeDestruct()
{
	if (AJGGameStateBase* GameState = BoundGameState.Get())
	{
		GameState->OnMatchStateChanged.RemoveDynamic(this, &UJGMatchHUDWidget::HandleMatchStateChanged);
	}

	BoundGameState.Reset();

	Super::NativeDestruct();
}

void UJGMatchHUDWidget::TryBindGameState()
{
	if (BoundGameState.IsValid())
	{
		return;
	}

	// on clients the game state may replicate after the HUD is created
	AJGGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState<AJGGameStateBase>() : nullptr;
	if (!GameState)
	{
		return;
	}

	BoundGameState = GameState;
	GameState->OnMatchStateChanged.AddDynamic(this, &UJGMatchHUDWidget::HandleMatchStateChanged);
	RefreshMatchState(GameState);
}

void UJGMatchHUDWidget::HandleMatchStateChanged(AJGGameStateBase* GameState)
{
	RefreshMatchState(GameState);
}

void UJGMatchHUDWidget::RefreshMatchState_Implementation(AJGGameStateBase* GameState)
{
	if (!GameState)
	{
		return;
	}

	const FJGMatchSnapshot Match = GameState->GetMatch();
	const EJGTeam LocalTeam = GetLocalTeam();

	if (ScoreText)
	{
		const FString MeA = LocalTeam == EJGTeam::A ? TEXT("(You) ") : TEXT("");
		const FString MeB = LocalTeam == EJGTeam::B ? TEXT(" (You)") : TEXT("");
		ScoreText->SetText(FText::FromString(FString::Printf(TEXT("%sA  %d : %d  B%s"), *MeA, Match.ScoreA, Match.ScoreB, *MeB)));
	}

	if (MessageText)
	{
		FString Message;

		if (Match.Phase == EJGMatchPhase::PointEnd)
		{
			const FString Who = Match.LastScoringTeam == LocalTeam ? TEXT("Your point") : TEXT("Opponent point");
			Message = FString::Printf(TEXT("%s - %s"), *Who, *ReasonName(Match.LastPointReason));
		}
		else if (Match.Phase == EJGMatchPhase::MatchEnd)
		{
			if (Match.EndReason == EJGMatchEndReason::PlayerLeft)
			{
				Message = TEXT("Opponent left the match");
			}
			else
			{
				Message = Match.WinnerTeam == LocalTeam ? TEXT("You win!") : FString::Printf(TEXT("Team %s wins"), *TeamName(Match.WinnerTeam));
			}
		}

		MessageText->SetText(FText::FromString(Message));
	}

	if (RematchButton)
	{
		const bool bShowRematch = Match.Phase == EJGMatchPhase::MatchEnd && Match.EndReason == EJGMatchEndReason::ScoreReached;
		RematchButton->SetVisibility(bShowRematch ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (AttackZone && Match.Phase != EJGMatchPhase::Rally && Match.Phase != EJGMatchPhase::Serving)
	{
		AttackZone->CancelActiveTouch();
	}
}

void UJGMatchHUDWidget::UpdatePhaseText(const AJGGameStateBase& GameState)
{
	if (!PhaseText)
	{
		return;
	}

	const EJGMatchPhase Phase = GameState.GetPhase();
	const float Remaining = GameState.GetPhaseRemainingSeconds();
	const bool bLocalServes = GameState.GetServingTeam() == GetLocalTeam();

	FString Text;

	switch (Phase)
	{
	case EJGMatchPhase::WaitingPlayers:
		Text = TEXT("Waiting for opponent...");
		break;

	case EJGMatchPhase::PreparingServe:
		Text = FString::Printf(TEXT("%s  %.1f"), bLocalServes ? TEXT("Your serve, get ready") : TEXT("Opponent serves"), Remaining);
		break;

	case EJGMatchPhase::Serving:
		Text = FString::Printf(TEXT("%s  %.0f"), bLocalServes ? TEXT("Serve! Tap or drag") : TEXT("Opponent serving"), FMath::CeilToFloat(Remaining));
		break;

	case EJGMatchPhase::MatchEnd:
		Text = TEXT("Match over");
		break;

	default:
		break;
	}

	PhaseText->SetText(FText::FromString(Text));
}

void UJGMatchHUDWidget::HandleRematchClicked()
{
	if (AJGPlayerController* PlayerController = Cast<AJGPlayerController>(GetOwningPlayer()))
	{
		PlayerController->RequestRematch();
	}

	if (RematchButton)
	{
		RematchButton->SetIsEnabled(false);
	}
}

void UJGMatchHUDWidget::SetTouchControlsVisible(bool bVisible)
{
	bTouchControlsVisible = bVisible;

	if (Joystick)
	{
		Joystick->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

EJGTeam UJGMatchHUDWidget::GetLocalTeam() const
{
	const APlayerController* PlayerController = GetOwningPlayer();
	const AJGPlayerState* PlayerState = PlayerController ? PlayerController->GetPlayerState<AJGPlayerState>() : nullptr;
	return PlayerState ? PlayerState->GetTeam() : EJGTeam::None;
}

FString UJGMatchHUDWidget::TeamName(EJGTeam Team)
{
	switch (Team)
	{
	case EJGTeam::A: return TEXT("A");
	case EJGTeam::B: return TEXT("B");
	default: return TEXT("-");
	}
}

FString UJGMatchHUDWidget::ReasonName(EJGPointReason Reason)
{
	switch (Reason)
	{
	case EJGPointReason::OutOnFirstLanding: return TEXT("Out");
	case EJGPointReason::FailedToCross: return TEXT("Did not cross the net");
	case EJGPointReason::NotReturned: return TEXT("Not returned");
	case EJGPointReason::ServeTimeout: return TEXT("Serve timeout");
	default: return TEXT("");
	}
}
