#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "JGLobbyGameMode.generated.h"

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

	/** 마지막으로 고른 선수의 외형을 입힌다. 저장된 선택이 없거나 로스터에 없으면 기본 선수다(UJGCharacterRoster). */
	virtual void RestartPlayer(AController* NewPlayer) override;
};
