#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Jokgu/Core/JGTypes.h"
#include "Jokgu/Player/JGStickMovable.h"
#include "JGCharacter.generated.h"

class UCameraComponent;
class UInputAction;
class UJGCharacterData;
class AJGBall;
class AJGGameStateBase;
struct FInputActionValue;

/**
 *  족구 플레이어: CharacterMovement 기반 이동, 공격 요청, 입력 저장, 슬라이딩, 고정 경기 카메라.
 *  키보드(Enhanced Input)와 터치 위젯 모두 DoMove / SubmitAttack / DoSlide를 호출한다.
 *  타격은 서버가 결정한다: 공에 대한 거리·높이 검사 후 공에 새 속도를 준다.
 */
UCLASS()
class AJGCharacter : public ACharacter, public IJGStickMovable
{
	GENERATED_BODY()

	/** 자기 진영 뒤쪽 위의 고정 카메라. 절대 트랜스폼을 쓰며 코트가 위치를 계산한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCameraComponent> MatchCamera;

protected:

#pragma region 입력 설정

	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> SlideAction;

	/** PC 테스트 입력: 탭 공격 */
	UPROPERTY(EditAnywhere, Category="Input|Debug")
	TObjectPtr<UInputAction> DebugTapAction;

	/** PC 테스트 입력: DebugDragPower로 정면 드래그 공격 */
	UPROPERTY(EditAnywhere, Category="Input|Debug")
	TObjectPtr<UInputAction> DebugDragAction;

	UPROPERTY(EditAnywhere, Category="Input|Debug", meta=(ClampMin=0, ClampMax=1))
	float DebugDragPower = 0.5f;

	/** 실제 화살표 메시나 데칼을 OnAimPreviewChanged에 연결하기 전까지 드래그 조준을 디버그 화살표로 그린다 */
	UPROPERTY(EditAnywhere, Category="Debug")
	bool bDrawPlaceholderAimArrow = true;

#pragma endregion

#pragma region 복제 상태

	/** 능력치, 메시, 몽타주를 담은 캐릭터 데이터. 서버가 정하고 복제되면 외형을 다시 적용한다. */
	UPROPERTY(ReplicatedUsing=OnRep_CharacterData, EditAnywhere, BlueprintReadOnly, Category="Jokgu")
	TObjectPtr<UJGCharacterData> CharacterData;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Jokgu")
	EJGSlideState SlideState = EJGSlideState::Ready;

#pragma endregion

public:

	AJGCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma region 조작 진입점

	/** 경기 카메라 기준 이동. Right/Forward는 [-1, 1]. */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoMove(float Right, float Forward);

	/** 가상 조이스틱 입력. DoMove와 같다. */
	virtual void DoStickMove(float Right, float Forward) override { DoMove(Right, Forward); }

	/** 마지막 유효 이동 방향으로 슬라이딩을 시작한다(움직인 적이 없으면 상대 쪽으로) */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoSlide();

	/** 확정된 탭/드래그를 서버로 보낸다. 로컬 플레이어 전용. */
	UFUNCTION(BlueprintCallable, Category="Input")
	void SubmitAttack(const FJGAttackInput& Input);

	/** 드래그 중 로컬 조준 화살표. 서버로 보낼 각도와 파워를 그대로 쓴다. */
	UFUNCTION(BlueprintCallable, Category="Input")
	void SetAimPreview(bool bVisible, float AimAngleDeg, float Power);

#pragma endregion

#pragma region 조회

	UFUNCTION(BlueprintPure, Category="Jokgu")
	EJGTeam GetTeam() const;

	/** 자기 코트에서 상대 코트를 향하는 방향 (캐릭터가 바라보는 방향이 아님) */
	UFUNCTION(BlueprintPure, Category="Jokgu")
	FVector GetAttackForward() const;

	UFUNCTION(BlueprintPure, Category="Jokgu")
	EJGSlideState GetSlideState() const { return SlideState; }

	UFUNCTION(BlueprintPure, Category="Jokgu")
	UJGCharacterData* GetCharacterData() const { return CharacterData; }

	/** 캡슐 바닥(발) 위치. 타격 범위와 서브 공 위치의 기준이다. */
	UFUNCTION(BlueprintPure, Category="Jokgu")
	FVector GetFootLocation() const;

#pragma endregion

#pragma region 서버 제어

	/** 서버: 데이터를 지정하고 능력치를 적용한다 */
	void SetCharacterData(UJGCharacterData* NewData);

	/** 서버: 새 득점을 위해 저장된 입력, 슬라이딩 상태, 타격 대기 시간을 지운다 */
	void ResetForNewPoint();

	/** 소유 클라이언트와 서버가 함께 쓰는 이동 규칙 (UJGCharacterMovementComponent 참고) */
	bool CanMoveNow() const;

#pragma endregion

protected:

	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

#pragma region 입력 처리

	void Move(const FInputActionValue& Value);
	void OnDebugTap();
	void OnDebugDrag();

#pragma endregion

#pragma region RPC와 블루프린트 이벤트

	UFUNCTION(Server, Reliable)
	void ServerSubmitAttack(const FJGAttackInput& Input);

	UFUNCTION(Server, Reliable)
	void ServerStartSlide(FVector_NetQuantizeNormal Direction);

	/** 서버가 득점 후 초기화할 때 소유 클라이언트의 예측 슬라이딩과 조준 표시도 지운다 */
	UFUNCTION(Client, Reliable)
	void ClientResetForNewPoint();

	/** 모든 기기에서 킥/슬라이딩 몽타주를 재생한다. 표시 전용이라 Unreliable이다. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayKickMotion(EJGKickMotion Motion);

	/** 킥 표시용 확장 지점 (몽타주는 캐릭터 데이터에서 자동으로 재생된다) */
	UFUNCTION(BlueprintImplementableEvent, Category="Jokgu")
	void OnKickMotionPlayed(EJGKickMotion Motion);

	/** 조준 화살표 표시용 확장 지점 (메시, 데칼, 위젯) */
	UFUNCTION(BlueprintImplementableEvent, Category="Jokgu")
	void OnAimPreviewChanged(bool bVisible, FVector WorldDirection, float Power);

	UFUNCTION()
	void OnRep_CharacterData();

	/** 이동 속도 능력치와 메시, 애님 클래스를 적용한다 */
	void ApplyCharacterData();

#pragma endregion

#pragma region 서버 타격 판정

	/** 공격 입력을 검증한 뒤 서브는 바로 치고, 랠리 공격은 입력 저장 슬롯에 넣는다 */
	void HandleAttackInputOnServer(const FJGAttackInput& Input);

	/** 매 틱 저장된 입력(슬라이딩 중이면 슬라이딩 수비)을 실행할 수 있는지 검사한다 */
	void TickServerHitCheck();

	bool TryExecuteBufferedAttack();
	void TryExecuteSlideSave();
	void ExecuteHit(AJGBall& Ball, const FJGAttackInput& Input, bool bIsServe);

	/** 발 기준 수평 거리와 공 중심 높이로 타격 범위를 검사한다 */
	bool IsBallInReach(const AJGBall& Ball, float HorizontalReach, float MinHeight, float MaxHeight) const;

	/** 캐릭터 데이터의 능력치를 읽는다. 데이터가 없으면 3(기본)이다. */
	int32 GetStat(int32 UJGCharacterData::* Stat) const;

	void ClearBufferedAttack();

#pragma endregion

#pragma region 슬라이딩

	void StartSlide(const FVector& Direction);
	void EndSlideActive();
	void EndSlideRecovery();
	void CancelSlide();

#pragma endregion

#pragma region 표시

	void UpdateMatchCamera(float DeltaSeconds);
	void DrawAimPreview() const;
	void DrawDebugReach() const;

	AJGGameStateBase* GetJGGameState() const;

#pragma endregion

#pragma region 내부 상태

	/** 서버: 공이 범위에 들어오기를 기다리는 공격 입력 하나 */
	FJGAttackInput BufferedAttack;
	double BufferedAttackExpireTime = 0.0;

	/** 서버: 타격 후 다시 칠 수 있는 시각 */
	double NextAttackAllowedTime = 0.0;

	/** 로컬: 다음 공격 요청에 붙일 순번 */
	int32 NextInputId = 0;

	/** 마지막 유효 이동 방향(수평 단위 벡터). 슬라이딩 방향으로 쓴다. */
	FVector LastMoveDirection = FVector::ZeroVector;
	FTimerHandle SlideTimer;

	/** 서버: 이번 슬라이딩에서 수비 타격을 이미 썼는지 (슬라이딩 한 번에 한 번) */
	bool bSlideSaveUsed = false;

	bool bAimPreviewVisible = false;
	float AimPreviewAngle = 0.0f;
	float AimPreviewPower = 0.0f;

	/** 로컬: 팀이 정해지거나 바뀌면 카메라를 보간 없이 바로 옮긴다 */
	bool bCameraInitialized = false;
	EJGTeam CameraTeam = EJGTeam::None;

#pragma endregion
};
