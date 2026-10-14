#include "Jokgu/Data/JGCharacterRoster.h"
#include "Jokgu/Data/JGCharacterData.h"
#include "Jokgu/Data/JGGameSettings.h"

const UJGCharacterRoster* UJGCharacterRoster::Get()
{
	if (const UJGGameSettings* Settings = GetDefault<UJGGameSettings>())
	{
		if (const UJGCharacterRoster* Roster = Settings->CharacterRoster.LoadSynchronous())
		{
			return Roster;
		}
	}

	return GetDefault<UJGCharacterRoster>();
}

UJGCharacterData* UJGCharacterRoster::Find(FName CharacterId) const
{
	for (UJGCharacterData* Data : Characters)
	{
		if (Data && Data->CharacterId == CharacterId)
		{
			return Data;
		}
	}

	return nullptr;
}

UJGCharacterData* UJGCharacterRoster::FindOrDefault(FName CharacterId) const
{
	UJGCharacterData* Data = Find(CharacterId);
	return Data ? Data : GetDefaultCharacter();
}

UJGCharacterData* UJGCharacterRoster::GetDefaultCharacter() const
{
	if (DefaultCharacter)
	{
		return DefaultCharacter;
	}

	return Characters.Num() > 0 ? Characters[0].Get() : nullptr;
}

UJGCharacterData* UJGCharacterRoster::GetAtOrDefault(int32 Index) const
{
	return Characters.IsValidIndex(Index) && Characters[Index] ? Characters[Index].Get() : GetDefaultCharacter();
}
