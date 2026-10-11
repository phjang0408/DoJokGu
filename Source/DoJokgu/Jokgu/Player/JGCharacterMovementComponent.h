#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "JGCharacterMovementComponent.generated.h"

/**
 *  AJGCharacter::CanMoveNow를 입력 가속에 적용한다. 소유 클라이언트는 예측할 때, 서버는 클라이언트 이동을
 *  재현할 때(MoveAutonomous) 이 함수를 거치므로 규칙을 무시한 클라이언트는 서버 보정으로 되돌아간다.
 */
UCLASS()
class UJGCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

protected:

	virtual FVector ConstrainInputAcceleration(const FVector& InputAcceleration) const override;
};
