#include "Jokgu/Lobby/JGLobbyGameMode.h"
#include "Jokgu/Core/JGSettingsSubsystem.h"
#include "Jokgu/Data/JGCharacterRoster.h"
#include "Jokgu/Lobby/JGLobbyCharacter.h"
#include "Jokgu/Lobby/JGLobbyPlayerController.h"

AJGLobbyGameMode::AJGLobbyGameMode()
{
	DefaultPawnClass = AJGLobbyCharacter::StaticClass();
	PlayerControllerClass = AJGLobbyPlayerController::StaticClass();
}

void AJGLobbyGameMode::RestartPlayer(AController* NewPlayer)
{
	Super::RestartPlayer(NewPlayer);

	AJGLobbyCharacter* Character = NewPlayer ? Cast<AJGLobbyCharacter>(NewPlayer->GetPawn()) : nullptr;
	const UJGSettingsSubsystem* Settings = UJGSettingsSubsystem::Get(this);

	if (Character)
	{
		Character->ApplyLook(UJGCharacterRoster::Get()->FindOrDefault(Settings ? Settings->GetLastCharacterId() : NAME_None));
	}
}
