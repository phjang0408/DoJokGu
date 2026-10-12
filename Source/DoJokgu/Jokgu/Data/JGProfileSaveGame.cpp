#include "Jokgu/Data/JGProfileSaveGame.h"
#include "Kismet/GameplayStatics.h"

const TCHAR* UJGProfileSaveGame::SlotName = TEXT("JGProfile");

UJGProfileSaveGame* UJGProfileSaveGame::Load()
{
	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		if (UJGProfileSaveGame* Loaded = Cast<UJGProfileSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0)))
		{
			return Loaded;
		}
	}

	return Cast<UJGProfileSaveGame>(UGameplayStatics::CreateSaveGameObject(UJGProfileSaveGame::StaticClass()));
}

bool UJGProfileSaveGame::Save()
{
	return UGameplayStatics::SaveGameToSlot(this, SlotName, 0);
}
