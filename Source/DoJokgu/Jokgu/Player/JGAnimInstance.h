#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "JGAnimInstance.generated.h"

class UAnimSequence;

/**
 *  DJG 스켈레톤이 함께 쓰는 제자리 이동 블렌드와 DefaultSlot 몽타주 그래프.
 *  애님 블루프린트 없이 C++ 프록시에서 노드를 직접 연결한다 (Idle-Walk-Run 블렌드 -> DefaultSlot).
 */
UCLASS(Transient, BlueprintType)
class DOJOKGU_API UJGAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UJGAnimInstance();

#pragma region 이동 애니메이션 설정

	UPROPERTY(EditDefaultsOnly, Category="Locomotion")
	TSoftObjectPtr<UAnimSequence> IdleAnimation;

	UPROPERTY(EditDefaultsOnly, Category="Locomotion")
	TSoftObjectPtr<UAnimSequence> WalkAnimation;

	UPROPERTY(EditDefaultsOnly, Category="Locomotion")
	TSoftObjectPtr<UAnimSequence> RunAnimation;

	/** 이 속도에서 걷기 애니메이션이 100%가 된다 */
	UPROPERTY(EditDefaultsOnly, Category="Locomotion", meta=(ClampMin="1"))
	float WalkSpeed = 180.f;

	/** 이 속도에서 달리기 애니메이션이 100%가 된다 */
	UPROPERTY(EditDefaultsOnly, Category="Locomotion", meta=(ClampMin="1"))
	float RunSpeed = 450.f;

#pragma endregion

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

private:
	friend struct FJGAnimInstanceProxy;

	// 프록시가 불러온 시퀀스를 GC에서 지키기 위한 참조
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> LoadedIdle;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> LoadedWalk;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> LoadedRun;
};
