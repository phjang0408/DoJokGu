#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "JGStudentCharacter.generated.h"

class UAnimMontage;
class UJGCharacterData;

/**
 *  로비에서 학교를 돌아다니는 AI 학생. 배치된 곳 주변의 NavMesh 위 임의 지점으로 걸어가 잠시 멈추고,
 *  가끔 감정표현 몽타주를 재생한다(행동은 AJGStudentAIController). 외형은 선수 데이터의 메시·애님 클래스를 쓴다.
 *  로비는 혼자 하는 Standalone이므로 복제하지 않는다.
 */
UCLASS()
class AJGStudentCharacter : public ACharacter
{
	GENERATED_BODY()

public:

	AJGStudentCharacter();

	/** 배치된 위치. 이 주변 WanderRadius 안에서만 돌아다닌다. */
	FVector GetHomeLocation() const { return HomeLocation; }

	float GetWanderRadius() const { return WanderRadius; }

	/** 멈춘 뒤 다음 이동까지 기다리는 시간 범위(초) */
	FVector2D GetWaitTimeRange() const { return WaitTimeRange; }

	/** 감정표현 몽타주를 하나 골라 재생한다. 재생한 길이(초), 없으면 0. */
	float PlayRandomEmote();

	/** 멈출 때마다 감정표현을 재생할 확률 */
	float GetEmoteChance() const { return EmoteChance; }

protected:

	/** 외형(관중용 05·07·08·09·10 재사용). 빌드 스크립트가 배치할 때 지정한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Student")
	TObjectPtr<UJGCharacterData> LookData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Student")
	TArray<TObjectPtr<UAnimMontage>> EmoteMontages;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Student", meta=(ClampMin="100"))
	float WanderRadius = 2500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Student")
	FVector2D WaitTimeRange = FVector2D(2.0, 6.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Student", meta=(ClampMin="0", ClampMax="1"))
	float EmoteChance = 0.3f;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

private:

	FVector HomeLocation = FVector::ZeroVector;

	void ApplyLook();
};
