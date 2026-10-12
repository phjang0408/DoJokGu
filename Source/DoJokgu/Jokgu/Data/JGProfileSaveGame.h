#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "JGProfileSaveGame.generated.h"

/**
 *  기기에 남는 개인 설정: 마지막으로 고른 선수, 볼륨 세 가지.
 *  읽고 쓰는 곳은 UJGSettingsSubsystem 하나뿐이다(여러 곳이 따로 저장하면 서로 덮어쓴다).
 *  그래픽 품질은 엔진의 GameUserSettings에 저장한다.
 */
UCLASS()
class UJGProfileSaveGame : public USaveGame
{
	GENERATED_BODY()

public:

	UPROPERTY(SaveGame)
	FName LastCharacterId;

	UPROPERTY(SaveGame)
	float MasterVolume = 1.0f;

	UPROPERTY(SaveGame)
	float EffectsVolume = 1.0f;

	UPROPERTY(SaveGame)
	float MusicVolume = 1.0f;

	/** 저장 파일을 읽는다. 없거나 읽지 못하면 기본값으로 새로 만든다. 항상 유효한 객체를 반환한다. */
	static UJGProfileSaveGame* Load();

	/** 이 값을 저장 슬롯에 쓴다 */
	bool Save();

	static const TCHAR* SlotName;
};
