#include "Jokgu/UI/JGMatchPanelWidget.h"
#include "Jokgu/Online/JGSessionSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"

namespace
{
	UJGSessionSubsystem* GetSessions(const UUserWidget* Widget)
	{
		const UGameInstance* GameInstance = Widget ? Widget->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UJGSessionSubsystem>() : nullptr;
	}
}

#pragma region 목록 버튼

void UJGIndexButton::Init(int32 InIndex, FJGIndexButtonClicked InOnClicked)
{
	Index = InIndex;
	OnIndexClicked = MoveTemp(InOnClicked);
	OnClicked.AddUniqueDynamic(this, &UJGIndexButton::HandleClicked);
}

void UJGIndexButton::HandleClicked()
{
	OnIndexClicked.ExecuteIfBound(Index);
}

#pragma endregion

#pragma region 초기화와 기본 레이아웃

void UJGMatchPanelWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}

	if (HostButton)
	{
		HostButton->OnClicked.AddUniqueDynamic(this, &UJGMatchPanelWidget::HandleHostClicked);
	}

	if (RefreshButton)
	{
		RefreshButton->OnClicked.AddUniqueDynamic(this, &UJGMatchPanelWidget::HandleRefreshClicked);
	}

	if (CloseButton)
	{
		CloseButton->OnClicked.AddUniqueDynamic(this, &UJGMatchPanelWidget::HandleCloseClicked);
	}

	if (UJGSessionSubsystem* Sessions = GetSessions(this))
	{
		Sessions->OnMatchSearchFinished.AddUniqueDynamic(this, &UJGMatchPanelWidget::HandleSearchFinished);
		Sessions->OnConnectionFailed.AddUniqueDynamic(this, &UJGMatchPanelWidget::HandleConnectionFailed);
	}
}

void UJGMatchPanelWidget::NativeDestruct()
{
	if (UJGSessionSubsystem* Sessions = GetSessions(this))
	{
		Sessions->OnMatchSearchFinished.RemoveDynamic(this, &UJGMatchPanelWidget::HandleSearchFinished);
		Sessions->OnConnectionFailed.RemoveDynamic(this, &UJGMatchPanelWidget::HandleConnectionFailed);
	}

	Super::NativeDestruct();
}

void UJGMatchPanelWidget::BuildDefaultLayout()
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
		Text->SetJustification(ETextJustify::Center);
		Text->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Text;
	};

	auto MakeButton = [&](const TCHAR* Label, const TCHAR* Name)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		Button->AddChild(MakeText(FString::Printf(TEXT("  %s  "), Label), 24, FLinearColor::Black));
		return Button;
	};

	auto AddRow = [Column](UWidget* Widget, float TopPadding)
	{
		if (UVerticalBoxSlot* RowSlot = Column->AddChildToVerticalBox(Widget))
		{
			RowSlot->SetHorizontalAlignment(HAlign_Center);
			RowSlot->SetPadding(FMargin(0.0f, TopPadding, 0.0f, 0.0f));
		}
	};

	AddRow(MakeText(TEXT("Play"), 30, FLinearColor::White), 0.0f);

	HostButton = MakeButton(TEXT("Create room"), TEXT("HostButton"));
	AddRow(HostButton, 14.0f);

	AddRow(MakeText(TEXT("Rooms on this Wi-Fi"), 22, FLinearColor::White), 20.0f);

	// 목록 칸 높이를 고정해서 결과 수가 바뀌어도 패널이 출렁이지 않게 한다
	USizeBox* ListBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ListBox->SetWidthOverride(460.0f);
	ListBox->SetMinDesiredHeight(160.0f);
	MatchList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MatchList"));
	ListBox->SetContent(MatchList);
	AddRow(ListBox, 8.0f);

	StatusText = MakeText(TEXT(""), 18, FLinearColor(0.85f, 0.85f, 0.85f));
	// 자동 줄바꿈은 글자 칸 자체 너비로 접혀서 좁아진다. 목록 칸 너비에 맞춰 접는다.
	StatusText->SetWrapTextAt(440.0f);
	AddRow(StatusText, 6.0f);

	UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	RefreshButton = MakeButton(TEXT("Refresh"), TEXT("RefreshButton"));
	CloseButton = MakeButton(TEXT("Close"), TEXT("CloseButton"));
	Buttons->AddChildToHorizontalBox(RefreshButton)->SetPadding(FMargin(8.0f, 0.0f));
	Buttons->AddChildToHorizontalBox(CloseButton)->SetPadding(FMargin(8.0f, 0.0f));
	AddRow(Buttons, 16.0f);
}

#pragma endregion

#pragma region 목록

void UJGMatchPanelWidget::Refresh()
{
	SetBusy(false);

	UJGSessionSubsystem* Sessions = GetSessions(this);
	if (!Sessions)
	{
		SetStatus(TEXT("Online features are not available."));
		return;
	}

	if (MatchList)
	{
		MatchList->ClearChildren();
	}

	SetStatus(TEXT("Searching..."));
	Sessions->FindLanMatches();
}

void UJGMatchPanelWidget::RebuildList()
{
	const UJGSessionSubsystem* Sessions = GetSessions(this);
	if (!MatchList || !Sessions)
	{
		return;
	}

	MatchList->ClearChildren();

	for (const FJGFoundMatch& Match : Sessions->GetFoundMatches())
	{
		UJGIndexButton* Button = WidgetTree->ConstructWidget<UJGIndexButton>(UJGIndexButton::StaticClass());
		Button->Init(Match.ResultIndex, FJGIndexButtonClicked::CreateUObject(this, &UJGMatchPanelWidget::HandleMatchPicked));

		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Label->GetFont();
		Font.Size = 22;
		Label->SetFont(Font);
		Label->SetText(FText::FromString(FString::Printf(TEXT("%s   (%d ms)  Join"), *Match.HostName, Match.PingMs)));
		Label->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
		Button->AddChild(Label);

		if (UVerticalBoxSlot* RowSlot = MatchList->AddChildToVerticalBox(Button))
		{
			RowSlot->SetPadding(FMargin(0.0f, 4.0f));
		}
	}
}

void UJGMatchPanelWidget::SetStatus(const FString& Status)
{
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(Status));
	}
}

#pragma endregion

#pragma region 이벤트

void UJGMatchPanelWidget::HandleSearchFinished(bool bSucceeded)
{
	const UJGSessionSubsystem* Sessions = GetSessions(this);
	RebuildList();

	if (!bSucceeded)
	{
		SetStatus(TEXT("Could not search for rooms. Check the Wi-Fi and try again."));
	}
	else if (!Sessions || Sessions->GetFoundMatches().Num() == 0)
	{
		SetStatus(TEXT("No rooms yet. Create one, or refresh."));
	}
	else
	{
		SetStatus(TEXT(""));
	}
}

void UJGMatchPanelWidget::HandleConnectionFailed(const FString& Reason)
{
	SetBusy(false);
	SetStatus(Reason);
}

void UJGMatchPanelWidget::HandleMatchPicked(int32 ResultIndex)
{
	if (UJGSessionSubsystem* Sessions = GetSessions(this))
	{
		SetBusy(true);
		SetStatus(TEXT("Joining..."));
		Sessions->JoinFoundMatch(ResultIndex);
	}
}

void UJGMatchPanelWidget::HandleHostClicked()
{
	if (UJGSessionSubsystem* Sessions = GetSessions(this))
	{
		SetBusy(true);
		SetStatus(TEXT("Opening the court..."));
		Sessions->HostDefaultMatch();
	}
}

void UJGMatchPanelWidget::HandleRefreshClicked()
{
	Refresh();
}

void UJGMatchPanelWidget::HandleCloseClicked()
{
	OnCloseRequested.Broadcast();
}

void UJGMatchPanelWidget::SetBusy(bool bBusy)
{
	for (UWidget* Widget : { static_cast<UWidget*>(HostButton.Get()), static_cast<UWidget*>(RefreshButton.Get()), static_cast<UWidget*>(MatchList.Get()) })
	{
		if (Widget)
		{
			Widget->SetIsEnabled(!bBusy);
		}
	}
}

#pragma endregion
