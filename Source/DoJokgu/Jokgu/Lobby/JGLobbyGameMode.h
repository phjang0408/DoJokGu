#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "JGLobbyGameMode.generated.h"

class UJGCharacterData;

/**
 *  혼자 돌아다니는 로비(Standalone). 경기 흐름은 없고, 플레이어를 PlayerStart(본관 앞 광장)에
 *  스폰해 마지막으로 고른 선수의 외형을 입힌다. 경기는 로비 HUD의 경기 패널이 세션 서브시스템으로 연다.
 */
UCLASS()
class AJGLobbyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	AJGLobbyGameMode();

	virtual void RestartPlayer(AController* NewPlayer) override;

protected:

	/** 외형을 고를 로스터. 경기 게임 모드와 같은 데이터 에셋을 넣는다. */
	UPROPERTY(EditDefaultsOnly, Category="Characters")
	TArray<TObjectPtr<UJGCharacterData>> CharacterRoster;

	/** 저장된 선택이 없거나 로스터에 없으면 쓰는 외형 */
	UPROPERTY(EditDefaultsOnly, Category="Characters")
	TObjectPtr<UJGCharacterData> DefaultCharacterData;

	const UJGCharacterData* FindLookData(FName CharacterId) const;
};
