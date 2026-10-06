#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "JGCharacterMovementComponent.generated.h"

/**
 *  Applies AJGCharacter::CanMoveNow to input acceleration. The owning client uses it for prediction and the
 *  server uses it when replaying client moves (MoveAutonomous), so a client that ignores the rule is corrected.
 */
UCLASS()
class UJGCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

protected:

	virtual FVector ConstrainInputAcceleration(const FVector& InputAcceleration) const override;
};
