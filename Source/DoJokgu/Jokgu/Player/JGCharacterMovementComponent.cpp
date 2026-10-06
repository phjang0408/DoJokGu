#include "Jokgu/Player/JGCharacterMovementComponent.h"
#include "Jokgu/Player/JGCharacter.h"

FVector UJGCharacterMovementComponent::ConstrainInputAcceleration(const FVector& InputAcceleration) const
{
	// slide velocity comes from a root motion source, so zero input does not stop an active slide
	const AJGCharacter* Character = Cast<AJGCharacter>(CharacterOwner);
	if (Character && !Character->CanMoveNow())
	{
		return FVector::ZeroVector;
	}

	return Super::ConstrainInputAcceleration(InputAcceleration);
}
