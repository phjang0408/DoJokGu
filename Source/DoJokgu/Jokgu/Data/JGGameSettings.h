#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "JGGameSettings.generated.h"

class UJGBalanceData;

/**
 *  Project Settings > Game > Jokgu
 *  Points the code at the balance asset so every system reads the same numbers.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Jokgu"))
class UJGGameSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	/** Balance asset used by every system. If unset, the UJGBalanceData class defaults are used. */
	UPROPERTY(Config, EditAnywhere, Category="Balance")
	TSoftObjectPtr<UJGBalanceData> BalanceData;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }
};
