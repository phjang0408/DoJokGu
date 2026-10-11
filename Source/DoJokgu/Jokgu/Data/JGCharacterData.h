#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Jokgu/Core/JGTypes.h"
#include "JGCharacterData.generated.h"

class USkeletalMesh;
class UAnimInstance;
class UAnimMontage;

/**
 *  캐릭터별 데이터. 캐릭터 사이의 차이는 데이터뿐이며 판정 코드는 같다.
 *  능력치는 GDD의 1-5 단계를 쓰고 UJGBalanceData 커브로 실제 값으로 바꾼다.
 */
UCLASS(BlueprintType)
class UJGCharacterData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

#pragma region 정보

	/** 로스터와 PlayerState에서 캐릭터를 찾는 키 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Identity")
	FName CharacterId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Identity")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Identity", meta=(MultiLine=true))
	FText RoleDescription;

#pragma endregion

#pragma region 능력치

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats", meta=(ClampMin=1, ClampMax=5))
	int32 MoveSpeedStat = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats", meta=(ClampMin=1, ClampMax=5))
	int32 KickPowerStat = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats", meta=(ClampMin=1, ClampMax=5))
	int32 AccuracyStat = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats", meta=(ClampMin=1, ClampMax=5))
	int32 ReachStat = 3;

#pragma endregion

#pragma region 외형

	/** 선택 항목. 비우면 캐릭터 블루프린트에 설정된 메시를 그대로 쓴다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visual")
	TObjectPtr<USkeletalMesh> SkeletalMesh;

	/** 선택 항목. 비우면 캐릭터 블루프린트에 설정된 애님 클래스를 그대로 쓴다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visual")
	TSubclassOf<UAnimInstance> AnimClass;

#pragma endregion

#pragma region 애니메이션

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

	/** 동작 슬롯의 몽타주를 반환한다. 킥 슬롯이 비어 있으면 중간 킥으로 대신한다. */
	UFUNCTION(BlueprintPure, Category="Animation")
	UAnimMontage* GetMontageForMotion(EJGKickMotion Motion) const;

#pragma endregion
};
