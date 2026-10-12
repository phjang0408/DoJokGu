#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "JGGameSettings.generated.h"

class UJGBalanceData;
class USoundClass;
class USoundMix;
class UWorld;

/**
 *  Project Settings > Game > Jokgu
 *  모든 시스템이 같은 수치를 읽도록 코드가 사용할 밸런스 에셋, 볼륨 조절용 사운드 에셋, 로비·경기 맵을 지정한다.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Jokgu"))
class UJGGameSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	/** 모든 시스템이 쓰는 밸런스 에셋. 비우면 UJGBalanceData 클래스 기본값을 쓴다. */
	UPROPERTY(Config, EditAnywhere, Category="Balance")
	TSoftObjectPtr<UJGBalanceData> BalanceData;

	/** 볼륨 설정을 걸 사운드 믹스. 비우면 볼륨 설정은 저장만 하고 소리에 적용하지 않는다. */
	UPROPERTY(Config, EditAnywhere, Category="Audio")
	TSoftObjectPtr<USoundMix> VolumeMix;

	/** 전체 볼륨. 효과음과 배경음 클래스의 부모여야 함께 줄어든다. */
	UPROPERTY(Config, EditAnywhere, Category="Audio")
	TSoftObjectPtr<USoundClass> MasterSoundClass;

	UPROPERTY(Config, EditAnywhere, Category="Audio")
	TSoftObjectPtr<USoundClass> EffectsSoundClass;

	UPROPERTY(Config, EditAnywhere, Category="Audio")
	TSoftObjectPtr<USoundClass> MusicSoundClass;

	/** 앱이 시작하고 경기가 끝나면 돌아가는 혼자 로비 맵 */
	UPROPERTY(Config, EditAnywhere, Category="Maps")
	TSoftObjectPtr<UWorld> LobbyMap;

	/** 로비에서 방을 만들 때 Listen Server로 여는 경기 맵 */
	UPROPERTY(Config, EditAnywhere, Category="Maps")
	TSoftObjectPtr<UWorld> MatchMap;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }
};
