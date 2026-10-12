#include "Jokgu/UI/JGCharacterSelectWidget.h"
#include "Jokgu/Core/JGGameStateBase.h"
#include "Jokgu/Core/JGPlayerState.h"
#include "Jokgu/Data/JGCharacterData.h"
#include "Jokgu/Player/JGPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"

#pragma region 로스터 버튼

void UJGRosterButton::Init(FName InCharacterId, FJGRosterButtonPicked InOnPicked)
{
	CharacterId = InCharacterId;
	OnPicked = MoveTemp(InOnPicked);
	OnClicked.AddUniqueDynamic(this, &UJGRosterButton::HandleClicked);
}

void UJGRosterButton::HandleClicked()
{
	OnPicked.ExecuteIfBound(CharacterId);
}

#pragma endregion

#pragma region 초기화와 기본 레이아웃

namespace
{
	const FLinearColor SelectedColor(1.0f, 0.75f, 0.2f);
	const FLinearColor IdleColor(1.0f, 1.0f, 1.0f);
}

void UJGCharacterSelectWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}

	if (ReadyButton)
	{
		ReadyButton->OnClicked.AddUniqueDynamic(this, &UJGCharacterSelectWidget::HandleReadyClicked);
	}
}

void UJGCharacterSelectWidget::BuildDefaultLayout()
{
	// 반투명 패널 하나에 제목, 선수 버튼 줄, 능력치, 준비 버튼, 상태 줄을 세로로 쌓는다
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	Panel->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));
	Panel->SetPadding(FMargin(24.0f, 16.0f));
	WidgetTree->RootWidget = Panel;

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	Panel->SetContent(Column);

	auto MakeText = [this](const TCHAR* Name, int32 FontSize)
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = FontSize;
		Text->SetFont(Font);
		Text->SetJustification(ETextJustify::Center);
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

	UTextBlock* Title = MakeText(TEXT("Title"), 28);
	Title->SetText(FText::FromString(TEXT("Choose your player")));
	AddRow(Title, 0.0f);

	RosterBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("RosterBox"));
	AddRow(RosterBox, 12.0f);

	InfoText = MakeText(TEXT("InfoText"), 20);
	AddRow(InfoText, 12.0f);

	ReadyButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ReadyButton"));
	ReadyLabel = MakeText(TEXT("ReadyLabel"), 26);
	ReadyLabel->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
	ReadyButton->AddChild(ReadyLabel);
	AddRow(ReadyButton, 14.0f);

	StatusText = MakeText(TEXT("StatusText"), 18);
	AddRow(StatusText, 10.0f);
}

void UJGCharacterSelectWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 준비 표시와 선택은 PlayerState로 따로 복제되므로 보일 때마다 매 틱 다시 읽는다 (글자 몇 줄뿐이다)
	const AJGGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState<AJGGameStateBase>() : nullptr;
	if (GameState)
	{
		TryBuildRosterButtons(*GameState);
		RefreshFromState(*GameState);
	}
}

void UJGCharacterSelectWidget::TryBuildRosterButtons(const AJGGameStateBase& GameState)
{
	if (!RosterBox || RosterButtons.Num() > 0 || GameState.GetCharacterRoster().Num() == 0)
	{
		return;
	}

	for (const UJGCharacterData* Data : GameState.GetCharacterRoster())
	{
		if (!Data)
		{
			continue;
		}

		UJGRosterButton* Button = WidgetTree->ConstructWidget<UJGRosterButton>(UJGRosterButton::StaticClass());
		Button->Init(Data->CharacterId, FJGRosterButtonPicked::CreateUObject(this, &UJGCharacterSelectWidget::HandleRosterPicked));

		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Label->GetFont();
		Font.Size = 24;
		Label->SetFont(Font);
		Label->SetText(Data->DisplayName.IsEmpty() ? FText::FromName(Data->CharacterId) : Data->DisplayName);
		Label->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
		Button->AddChild(Label);

		// 손가락으로 누르기 쉽도록 버튼 사이를 띄운다
		if (UHorizontalBoxSlot* ButtonSlot = RosterBox->AddChildToHorizontalBox(Button))
		{
			ButtonSlot->SetPadding(FMargin(8.0f, 0.0f));
		}

		RosterButtons.Add(Button);
	}
}

#pragma endregion

#pragma region 상태 표시

void UJGCharacterSelectWidget::RefreshFromState(const AJGGameStateBase& GameState)
{
	const AJGPlayerState* Local = GetLocalPlayerState();
	const AJGPlayerState* Opponent = FindOpponentState(GameState);

	const FName LocalPick = Local ? Local->GetCharacterId() : NAME_None;
	const bool bLocalReady = Local && Local->IsReady();
	const bool bCanReady = Local && Local->GetTeam() != EJGTeam::None;

	// 준비한 뒤에는 선택을 잠근다 (서버도 같은 이유로 거절한다)
	for (UJGRosterButton* Button : RosterButtons)
	{
		Button->SetBackgroundColor(Button->GetCharacterId() == LocalPick ? SelectedColor : IdleColor);
		Button->SetIsEnabled(!bLocalReady);
	}

	if (InfoText)
	{
		InfoText->SetText(DescribeCharacter(GameState.FindRosterCharacter(LocalPick)));
	}

	if (ReadyButton)
	{
		ReadyButton->SetIsEnabled(bCanReady);
	}

	if (ReadyLabel)
	{
		ReadyLabel->SetText(FText::FromString(bLocalReady ? TEXT("  Cancel  ") : TEXT("  Ready  ")));
	}

	if (StatusText)
	{
		auto Describe = [&GameState](const AJGPlayerState* PlayerState)
		{
			const UJGCharacterData* Data = GameState.FindRosterCharacter(PlayerState->GetCharacterId());
			const FString Name = Data ? Data->DisplayName.ToString() : PlayerState->GetCharacterId().ToString();
			return FString::Printf(TEXT("%s - %s"), *Name, PlayerState->IsReady() ? TEXT("Ready") : TEXT("Choosing"));
		};

		const FString Mine = Local ? Describe(Local) : TEXT("-");
		const FString Theirs = Opponent ? Describe(Opponent) : TEXT("waiting for an opponent...");
		StatusText->SetText(FText::FromString(FString::Printf(TEXT("You: %s     Opponent: %s"), *Mine, *Theirs)));
	}
}

FText UJGCharacterSelectWidget::DescribeCharacter(const UJGCharacterData* Data)
{
	if (!Data)
	{
		return FText::GetEmpty();
	}

	FString Line = FString::Printf(TEXT("Speed %d   Power %d   Accuracy %d   Reach %d"),
		Data->MoveSpeedStat, Data->KickPowerStat, Data->AccuracyStat, Data->ReachStat);

	if (!Data->RoleDescription.IsEmpty())
	{
		Line = Data->RoleDescription.ToString() + TEXT("\n") + Line;
	}

	return FText::FromString(Line);
}

#pragma endregion

#pragma region 요청과 도우미

void UJGCharacterSelectWidget::HandleRosterPicked(FName CharacterId)
{
	if (AJGPlayerController* PlayerController = Cast<AJGPlayerController>(GetOwningPlayer()))
	{
		PlayerController->RequestSelectCharacter(CharacterId);
	}
}

void UJGCharacterSelectWidget::HandleReadyClicked()
{
	const AJGPlayerState* Local = GetLocalPlayerState();
	AJGPlayerController* PlayerController = Cast<AJGPlayerController>(GetOwningPlayer());

	if (Local && PlayerController)
	{
		PlayerController->RequestSetReady(!Local->IsReady());
	}
}

AJGPlayerState* UJGCharacterSelectWidget::GetLocalPlayerState() const
{
	const APlayerController* PlayerController = GetOwningPlayer();
	return PlayerController ? PlayerController->GetPlayerState<AJGPlayerState>() : nullptr;
}

const AJGPlayerState* UJGCharacterSelectWidget::FindOpponentState(const AJGGameStateBase& GameState) const
{
	const AJGPlayerState* Local = GetLocalPlayerState();

	for (const APlayerState* PlayerState : GameState.PlayerArray)
	{
		const AJGPlayerState* JGPlayerState = Cast<AJGPlayerState>(PlayerState);
		if (JGPlayerState && JGPlayerState != Local && JGPlayerState->GetTeam() != EJGTeam::None)
		{
			return JGPlayerState;
		}
	}

	return nullptr;
}

#pragma endregion
