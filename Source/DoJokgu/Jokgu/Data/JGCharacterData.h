#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Jokgu/Core/JGTypes.h"
#include "JGCharacterData.generated.h"

class USkeletalMesh;
class UAnimInstance;
class UAnimMontage;

/**
 *  Per character data. Characters only differ by data, never by judgement code.
 *  Stats use the 1-5 scale of the GDD and are converted through UJGBalanceData curves.
 */
UCLASS(BlueprintType)
class UJGCharacterData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Identity")
	FName CharacterId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Identity")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Identity", meta=(MultiLine=true))
	FText RoleDescription;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats", meta=(ClampMin=1, ClampMax=5))
	int32 MoveSpeedStat = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats", meta=(ClampMin=1, ClampMax=5))
	int32 KickPowerStat = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats", meta=(ClampMin=1, ClampMax=5))
	int32 AccuracyStat = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats", meta=(ClampMin=1, ClampMax=5))
	int32 ReachStat = 3;

	/** Optional. If unset, the mesh configured on the character Blueprint is kept. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visual")
	TObjectPtr<USkeletalMesh> SkeletalMesh;

	/** Optional. If unset, the anim class configured on the character Blueprint is kept. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visual")
	TSubclassOf<UAnimInstance> AnimClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Animation")
	TObjectPtr<UAnimMontage> TapMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Animation")
	TObjectPtr<UAnimMontage> WeakKickMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Animation")
	TObjectPtr<UAnimMontage> MediumKickMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Animation")
	TObjectPtr<UAnimMontage> StrongKickMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Animation")
	TObjectPtr<UAnimMontage> ServeMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Animation")
	TObjectPtr<UAnimMontage> SlideMontage;

	/** Returns the montage for a motion slot, falling back to the medium kick for empty kick slots */
	UFUNCTION(BlueprintPure, Category="Animation")
	UAnimMontage* GetMontageForMotion(EJGKickMotion Motion) const;
};
