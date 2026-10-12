#include "Jokgu/Lobby/JGLobbyPlayerController.h"
#include "Jokgu/UI/JGLobbyHUDWidget.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "DoJokgu.h"

void AJGLobbyPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalPlayerController())
	{
		return;
	}

	const TSubclassOf<UJGLobbyHUDWidget> WidgetClass = HUDWidgetClass ? HUDWidgetClass : TSubclassOf<UJGLobbyHUDWidget>(UJGLobbyHUDWidget::StaticClass());
	HUDWidget = CreateWidget<UJGLobbyHUDWidget>(this, WidgetClass);

	if (HUDWidget)
	{
		HUDWidget->SetTouchControlsVisible(ShouldUseTouchControls());
		HUDWidget->AddToPlayerScreen(0);
	}
	else
	{
		UE_LOG(LogDoJokgu, Error, TEXT("Could not create the lobby HUD."));
	}

	// 패널 버튼과 카메라 드래그를 마우스로도 쓸 수 있게 한다
	bShowMouseCursor = true;

	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
}

void AJGLobbyPlayerController::SetupInputComponent()
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
}

bool AJGLobbyPlayerController::ShouldUseTouchControls() const
{
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

void AJGLobbyPlayerController::SetZoneOverlap(AJGInteractZone* Zone, bool bEntered)
{
	OverlappingZones.RemoveAll([Zone](const TWeakObjectPtr<AJGInteractZone>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == Zone;
	});

	if (bEntered)
	{
		OverlappingZones.Add(Zone);
	}

	RefreshZoneButton();
}

void AJGLobbyPlayerController::RefreshZoneButton()
{
	if (!HUDWidget)
	{
		return;
	}

	const AJGInteractZone* Zone = OverlappingZones.Num() > 0 ? OverlappingZones.Last().Get() : nullptr;
	HUDWidget->SetActiveZone(Zone);
}
