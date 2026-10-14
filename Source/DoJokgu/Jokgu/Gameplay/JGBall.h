#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Jokgu/Core/JGTypes.h"
#include "JGBall.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class AJGBall;

/** 서버 전용 알림. 접촉 위치를 함께 넘긴다. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FJGBallContactDelegate, AJGBall* /*Ball*/, const FVector& /*Location*/);

/**
 *  경기 공: 구 충돌체 루트 + ProjectileMovement (물리 시뮬레이션 없음).
 *  위치, 접촉, 랠리 상태는 서버가 가진다. 클라이언트는 복제된 이동을 받아 같은 ProjectileMovement로 외삽한다.
 *  서버 보정은 보이는 메시의 오프셋으로 흡수해 짧은 시간 동안 줄여 나가고, 새 샷이나 초기화 때는
 *  보이는 메시를 즉시 맞춰서 이전 궤적이 새 궤적을 끌고 가지 않게 한다.
 *  캐릭터는 공과 물리적으로 닿지 않는다: 타격은 범위 검사 후 새 속도를 주는 방식이다.
 */
UCLASS()
class AJGBall : public AActor
{
	GENERATED_BODY()

#pragma region 컴포넌트

	/** 판정용 구 충돌체. JGBall 채널만 쓰므로 코트 바닥과 네트에만 튕긴다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USphereComponent> Collision;

	/** 보이는 공. 클라이언트에서는 보정 오프셋만큼 충돌체에서 떨어져 있을 수 있다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> VisualMesh;

	/** 공의 지면 투영. 게임 안에서 공 위치를 알려 주는 유일한 단서이며, 미래 낙하지점은 보여 주지 않는다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> ShadowMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UProjectileMovementComponent> Movement;

#pragma endregion

protected:

#pragma region 설정과 복제 상태

	/** 보이는 반지름. 판정은 언제나 UJGBalanceData::BallRadius를 쓴다. */
	UPROPERTY(EditAnywhere, Category="Ball|Visual", meta=(ClampMin=1, Units="cm"))
	float VisualRadius = 13.0f;

	UPROPERTY(EditAnywhere, Category="Ball|Visual")
	FLinearColor BallColor = FLinearColor(0.95f, 0.85f, 0.10f);

	UPROPERTY(EditAnywhere, Category="Ball|Visual")
	FLinearColor ShadowColor = FLinearColor(0.02f, 0.02f, 0.02f);

	/** 이 시간 안에 다시 보고된 바닥 접촉은 무시한다 */
	UPROPERTY(EditAnywhere, Category="Ball", meta=(ClampMin=0, Units="s"))
	float MinGroundContactInterval = 0.05f;

	/** 공이 날아가는 중인지. 서브를 위해 들고 있을 때는 false다. */
	UPROPERTY(ReplicatedUsing=OnRep_InPlay, VisibleInstanceOnly, BlueprintReadOnly, Category="Ball")
	bool bInPlay = false;

	UPROPERTY(ReplicatedUsing=OnRep_RallyState, VisibleInstanceOnly, BlueprintReadOnly, Category="Ball")
	FJGRallyState RallyState;

	/** 클라이언트: 보이는 메시 보정이 줄어드는 시간 상수 */
	UPROPERTY(EditAnywhere, Category="Ball|Network", meta=(ClampMin=0, Units="s"))
	float VisualSmoothingTime = 0.08f;

	/** 클라이언트: 이보다 큰 보정은 부드럽게 따라가지 않고 즉시 맞춘다 */
	UPROPERTY(EditAnywhere, Category="Ball|Network", meta=(ClampMin=0, Units="cm"))
	float VisualSnapDistance = 150.0f;

#pragma endregion

public:

	AJGBall();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PostNetReceiveVelocity(const FVector& NewVelocity) override;
	virtual void PostNetReceiveLocationAndRotation() override;

#pragma region 서버 전용 이벤트와 제어

	/** 서버: 지면 접촉 (JGGround 태그가 붙은 바닥, 또는 경기 영역 밖으로 떨어짐) */
	FJGBallContactDelegate OnGroundContact;

	/** 서버: 공이 멈춤 */
	FJGBallContactDelegate OnStopped;

	/** 서버: 네트 평면(코트 로컬 X = 0) 통과를 찾는 데 쓰는 코트 트랜스폼 */
	void SetCourtFrame(const FTransform& InCourtFrame);

	/** 서버: 공을 한 위치에 고정한다(예: 서브하는 사람 앞). 랠리를 지운다. */
	void HoldAt(const FVector& Location);

	/** 서버: 타격을 적용한다. 친 팀을 등록하고 주어진 속도로 발사한다. */
	void LaunchByHit(const FVector& Velocity, EJGTeam HitterTeam);

	/** 서버: 공은 계속 움직이게 두고 판정만 멈춘다(득점 결정 후) */
	void ClearRally();

#pragma endregion

#pragma region 조회

	UFUNCTION(BlueprintPure, Category="Ball")
	bool IsInPlay() const { return bInPlay; }

	/** 공이 날아가는 중이고 이 팀이 받을 차례면 true */
	UFUNCTION(BlueprintPure, Category="Ball")
	bool IsHittableBy(EJGTeam Team) const;

	UFUNCTION(BlueprintPure, Category="Ball")
	const FJGRallyState& GetRallyState() const { return RallyState; }

	/** 서버: 게임 모드가 지면 접촉 판정 결과를 직접 기록할 때 쓴다 */
	FJGRallyState& GetMutableRallyState() { return RallyState; }

	/** 중력 배율이 적용된 공의 중력 가속도 Z (음수) */
	UFUNCTION(BlueprintPure, Category="Ball")
	float GetBallGravityZ() const;

	UFUNCTION(BlueprintPure, Category="Ball")
	FVector GetBallVelocity() const;

	/** 화면에 보이는 공 위치. 클라이언트에서는 서버 보정을 부드럽게 따라가는 중이라 충돌체 위치와 다를 수 있다. */
	FVector GetVisualLocation() const;

#pragma endregion

protected:

	virtual void BeginPlay() override;

#pragma region 내부 처리

	UFUNCTION()
	void HandleBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity);

	UFUNCTION()
	void HandleStop(const FHitResult& ImpactResult);

	UFUNCTION()
	void OnRep_InPlay();

	UFUNCTION()
	void OnRep_RallyState();

	void SetMovementActive(bool bActive);

	/** 밸런스 에셋의 반지름, 중력, 반발, 마찰을 컴포넌트에 적용한다 */
	void ApplyBalance();

	void UpdateShadow();
	void UpdateNetCrossing();
	void UpdateVisualSmoothing(float DeltaSeconds);
	void SnapVisual();
	void DrawDebugTrajectory() const;

#pragma endregion

#pragma region 내부 상태

	/** 서버: 마지막 바닥 접촉 시각. 짧은 간격의 중복 접촉을 거른다. */
	double LastGroundContactTime = -1.0;

	/** 서버: 경기 영역 밖 낙하를 이미 알렸는지 */
	bool bOutOfPlayReported = false;

	FTransform CourtFrame = FTransform::Identity;
	bool bHasCourtFrame = false;

	/** 서버: 직전 틱의 코트 로컬 위치. 네트 통과 보간에 쓴다. */
	FVector PreviousCourtLocal = FVector::ZeroVector;

	/** 클라이언트: 마지막으로 본 샷 번호. 새 샷이면 보이는 메시를 즉시 맞춘다. */
	int32 LastSeenShotId = 0;

	/** 클라이언트: 다음 위치 갱신에서 보정 오프셋을 쓰지 않고 바로 맞춘다 */
	bool bSnapNextNetUpdate = false;

#pragma endregion
};
