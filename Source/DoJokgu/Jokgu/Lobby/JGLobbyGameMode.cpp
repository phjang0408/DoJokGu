#include "Jokgu/Lobby/JGLobbyGameMode.h"
#include "Jokgu/Core/JGSettingsSubsystem.h"
#include "Jokgu/Data/JGCharacterData.h"
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
		Character->ApplyLook(FindLookData(Settings ? Settings->GetLastCharacterId() : NAME_None));
	}
}

const UJGCharacterData* AJGLobbyGameMode::FindLookData(FName CharacterId) const
{
	for (const UJGCharacterData* Data : CharacterRoster)
	{
		if (Data && Data->CharacterId == CharacterId)
		{
			return Data;
		}
	}

	return DefaultCharacterData;
}
