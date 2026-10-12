#include "Jokgu/Player/JGPlayerController.h"
#include "Jokgu/Core/JGGameModeBase.h"
#include "Jokgu/Core/JGSettingsSubsystem.h"
#include "Jokgu/UI/JGMatchHUDWidget.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "DoJokgu.h"

#pragma region 초기화

void AJGPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Listen Server의 Host에는 원격 플레이어 컨트롤러도 있으므로 로컬 컨트롤러만 UI를 만든다
	if (!IsLocalPlayerController())
	{
		return;
	}

	const TSubclassOf<UJGMatchHUDWidget> WidgetClass = HUDWidgetClass ? HUDWidgetClass : TSubclassOf<UJGMatchHUDWidget>(UJGMatchHUDWidget::StaticClass());
	HUDWidget = CreateWidget<UJGMatchHUDWidget>(this, WidgetClass);

	if (HUDWidget)
	{
		HUDWidget->SetTouchControlsVisible(ShouldUseTouchControls());
		HUDWidget->AddToPlayerScreen(0);
	}
	else
	{
		UE_LOG(LogDoJokgu, Error, TEXT("Could not create the match HUD."));
	}

	// 공격 영역은 마우스 입력도 받아서 PC에서 드래그를 시험할 수 있다
	bShowMouseCursor = true;

	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);

	// 지난번에 고른 선수로 시작한다. 로스터에 없거나 선택 단계가 아니면 서버가 무시한다.
	const UJGSettingsSubsystem* Settings = UJGSettingsSubsystem::Get(this);
	if (Settings && !Settings->GetLastCharacterId().IsNone())
	{
		ServerSelectCharacter(Settings->GetLastCharacterId());
	}
}

void AJGPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!IsLocalPlayerController())
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		for (UInputMappingContext* Context : DefaultMappingContexts)
		{
			if (Context)
			{
				Subsystem->AddMappingContext(Context, 0);
			}
		}
	}

	// 재경기는 Pawn이 아니라 컨트롤러에 묶는다
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (RematchAction)
		{
			EnhancedInputComponent->BindAction(RematchAction, ETriggerEvent::Started, this, &AJGPlayerController::RequestRematch);
		}
	}
}

bool AJGPlayerController::ShouldUseTouchControls() const
{
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

#pragma endregion

#pragma region 경기 요청

void AJGPlayerController::RequestRematch()
{
	ServerRequestRematch();
}

void AJGPlayerController::ServerRequestRematch_Implementation()
{
	if (AJGGameModeBase* GameMode = GetWorld()->GetAuthGameMode<AJGGameModeBase>())
	{
		GameMode->HandleRematchRequest(this);
	}
}

void AJGPlayerController::RequestSelectCharacter(FName CharacterId)
{
	ServerSelectCharacter(CharacterId);

	// 서버가 거절해도 다음 경기에서 다시 시도할 뿐이므로 요청한 값을 그대로 저장한다
	if (UJGSettingsSubsystem* Settings = UJGSettingsSubsystem::Get(this))
	{
		Settings->SetLastCharacterId(CharacterId);
	}
}

void AJGPlayerController::RequestSetReady(bool bReady)
{
	ServerSetReady(bReady);
}

void AJGPlayerController::JGCharacter(FName CharacterId)
{
	RequestSelectCharacter(CharacterId);
}

void AJGPlayerController::JGReady(bool bReady)
{
	RequestSetReady(bReady);
}

void AJGPlayerController::ServerSetReady_Implementation(bool bReady)
{
	if (AJGGameModeBase* GameMode = GetWorld()->GetAuthGameMode<AJGGameModeBase>())
	{
		GameMode->HandleReadyRequest(this, bReady);
	}
}

void AJGPlayerController::ServerSelectCharacter_Implementation(FName CharacterId)
{
	if (AJGGameModeBase* GameMode = GetWorld()->GetAuthGameMode<AJGGameModeBase>())
	{
		GameMode->HandleCharacterSelect(this, CharacterId);
	}
}

#pragma endregion
