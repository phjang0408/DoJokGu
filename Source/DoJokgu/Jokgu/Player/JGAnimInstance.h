#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "JGAnimInstance.generated.h"

class UAnimSequence;

/** Shared in-place locomotion and DefaultSlot montage graph for the DJG skeleton. */
UCLASS(Transient, BlueprintType)
class DOJOKGU_API UJGAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UJGAnimInstance();

	UPROPERTY(EditDefaultsOnly, Category="Locomotion")
	TSoftObjectPtr<UAnimSequence> IdleAnimation;

	UPROPERTY(EditDefaultsOnly, Category="Locomotion")
	TSoftObjectPtr<UAnimSequence> WalkAnimation;

	UPROPERTY(EditDefaultsOnly, Category="Locomotion")
	TSoftObjectPtr<UAnimSequence> RunAnimation;

	UPROPERTY(EditDefaultsOnly, Category="Locomotion", meta=(ClampMin="1"))
	float WalkSpeed = 180.f;

	UPROPERTY(EditDefaultsOnly, Category="Locomotion", meta=(ClampMin="1"))
	float RunSpeed = 450.f;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

private:
	friend struct FJGAnimInstanceProxy;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> LoadedIdle;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> LoadedWalk;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> LoadedRun;
};
