#include "Jokgu/Player/JGCharacterMovementComponent.h"
#include "Jokgu/Player/JGCharacter.h"

FVector UJGCharacterMovementComponent::ConstrainInputAcceleration(const FVector& InputAcceleration) const
{
	// 슬라이딩 속도는 RootMotionSource에서 오므로 입력을 0으로 만들어도 진행 중인 슬라이딩은 멈추지 않는다
	const AJGCharacter* Character = Cast<AJGCharacter>(CharacterOwner);
	if (Character && !Character->CanMoveNow())
	{
		return FVector::ZeroVector;
	}

	return Super::ConstrainInputAcceleration(InputAcceleration);
}
