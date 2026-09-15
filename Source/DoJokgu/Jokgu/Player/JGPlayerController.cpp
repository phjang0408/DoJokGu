#include "Jokgu/Player/JGPlayerController.h"
#include "Jokgu/Core/JGGameModeBase.h"
#include "Jokgu/UI/JGMatchHUDWidget.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "DoJokgu.h"

void AJGPlayerController::BeginPlay()
{
	Super::BeginPlay();

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

	// the attack zone also accepts the mouse so drags can be tested on PC
	bShowMouseCursor = true;

	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
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
