#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "JGGameSettings.generated.h"

class UJGBalanceData;

/**
 *  Project Settings > Game > Jokgu
 *  모든 시스템이 같은 수치를 읽도록 코드가 사용할 밸런스 에셋을 지정한다.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Jokgu"))
class UJGGameSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	/** 모든 시스템이 쓰는 밸런스 에셋. 비우면 UJGBalanceData 클래스 기본값을 쓴다. */
	UPROPERTY(Config, EditAnywhere, Category="Balance")
	TSoftObjectPtr<UJGBalanceData> BalanceData;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }
};
