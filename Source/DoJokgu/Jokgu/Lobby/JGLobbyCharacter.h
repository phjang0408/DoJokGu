#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Jokgu/Player/JGStickMovable.h"
#include "JGLobbyCharacter.generated.h"

class UCameraComponent;
class UInputAction;
class USpringArmComponent;
class UJGCharacterData;
struct FInputActionValue;

/**
 *  로비에서 학교를 걸어 다니는 3인칭 캐릭터. 걷기·달리기·점프만 하고 공격·슬라이딩 로직은 없다.
 *  경기의 공격 키(J, 패드 아래 버튼)와 Space는 로비에서 점프다(IMC_Lobby).
 *  스틱을 조금 밀면 걷고 끝까지 밀면 달린다. 카메라는 화면을 끌어 좌우로 돌린다.
 *  외형은 마지막으로 고른 선수의 데이터(메시·애님 클래스)를 쓴다.
 */
UCLASS()
class AJGLobbyCharacter : public ACharacter, public IJGStickMovable
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCameraComponent> FollowCamera;

public:

	AJGLobbyCharacter();

	/** 선수 외형을 입힌다. 비어 있는 항목은 블루프린트 기본값을 그대로 둔다. */
	void ApplyLook(const UJGCharacterData* Data);

	/** 카메라 기준 이동. Right/Forward는 [-1, 1]. */
	virtual void DoStickMove(float Right, float Forward) override;

	/** 화면 드래그로 카메라를 돌린다. 양수면 오른쪽으로 돈다. */
	void AddCameraYaw(float Degrees);

	/** 점프 버튼을 눌렀다/뗐다. 터치 점프 버튼도 이 둘을 부른다. */
	UFUNCTION(BlueprintCallable, Category="Lobby")
	void DoJumpStart();

	UFUNCTION(BlueprintCallable, Category="Lobby")
	void DoJumpEnd();

protected:

	/** 키보드 이동 (경기와 같은 IA_Move) */
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> MoveAction;

	/** 점프 (IA_Jump) */
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> JumpAction;

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	void Move(const FInputActionValue& Value);
};
