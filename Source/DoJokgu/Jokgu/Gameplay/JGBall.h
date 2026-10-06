#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Jokgu/Core/JGTypes.h"
#include "JGBall.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class AJGBall;

/** Server-only notification with the contact location */
DECLARE_MULTICAST_DELEGATE_TwoParams(FJGBallContactDelegate, AJGBall* /*Ball*/, const FVector& /*Location*/);

/**
 *  Match ball: sphere collision root + ProjectileMovement (no physics simulation).
 *  The server owns position, contacts and rally state. Clients receive replicated movement and extrapolate with the
 *  same projectile movement. Corrections are absorbed by an offset on the visual mesh that decays over a short time,
 *  and a new shot or a reset snaps the visual so an old trajectory never drags the new one.
 *  Characters never touch the ball physically: hits are range checks that set a new velocity.
 */
UCLASS()
class AJGBall : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> VisualMesh;

	/** Ground projection of the ball, the only in-game hint about its position. Future landing spots are never shown. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> ShadowMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UProjectileMovementComponent> Movement;

protected:

	/** Visual radius. Judgement always uses UJGBalanceData::BallRadius. */
	UPROPERTY(EditAnywhere, Category="Ball|Visual", meta=(ClampMin=1, Units="cm"))
	float VisualRadius = 13.0f;

	UPROPERTY(EditAnywhere, Category="Ball|Visual")
	FLinearColor BallColor = FLinearColor(0.95f, 0.85f, 0.10f);

	UPROPERTY(EditAnywhere, Category="Ball|Visual")
	FLinearColor ShadowColor = FLinearColor(0.02f, 0.02f, 0.02f);

	/** Ignores extra floor contacts reported within this time */
	UPROPERTY(EditAnywhere, Category="Ball", meta=(ClampMin=0, Units="s"))
	float MinGroundContactInterval = 0.05f;

	UPROPERTY(ReplicatedUsing=OnRep_InPlay, VisibleInstanceOnly, BlueprintReadOnly, Category="Ball")
	bool bInPlay = false;

	UPROPERTY(ReplicatedUsing=OnRep_RallyState, VisibleInstanceOnly, BlueprintReadOnly, Category="Ball")
	FJGRallyState RallyState;

	/** Client: time constant of the visual correction decay */
	UPROPERTY(EditAnywhere, Category="Ball|Network", meta=(ClampMin=0, Units="s"))
	float VisualSmoothingTime = 0.08f;

	/** Client: corrections larger than this snap instead of being smoothed */
	UPROPERTY(EditAnywhere, Category="Ball|Network", meta=(ClampMin=0, Units="cm"))
	float VisualSnapDistance = 150.0f;

public:

	AJGBall();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PostNetReceiveVelocity(const FVector& NewVelocity) override;
	virtual void PostNetReceiveLocationAndRotation() override;

	/** Server: ground contact (floor tagged JGGround, or out of the world) */
	FJGBallContactDelegate OnGroundContact;

	/** Server: projectile came to rest */
	FJGBallContactDelegate OnStopped;

	/** Server: court transform used to detect the net plane crossing (court local X = 0) */
	void SetCourtFrame(const FTransform& InCourtFrame);

	/** Server: freezes the ball at a location, e.g. in front of the server. Clears the rally. */
	void HoldAt(const FVector& Location);

	/** Server: applies a hit. Registers the hitter and launches with the given velocity. */
	void LaunchByHit(const FVector& Velocity, EJGTeam HitterTeam);

	/** Server: keeps the ball moving but stops judging it (point decided) */
	void ClearRally();

	UFUNCTION(BlueprintPure, Category="Ball")
	bool IsInPlay() const { return bInPlay; }

	UFUNCTION(BlueprintPure, Category="Ball")
	bool IsHittableBy(EJGTeam Team) const;

	UFUNCTION(BlueprintPure, Category="Ball")
	const FJGRallyState& GetRallyState() const { return RallyState; }

	FJGRallyState& GetMutableRallyState() { return RallyState; }

	UFUNCTION(BlueprintPure, Category="Ball")
	float GetBallGravityZ() const;

	UFUNCTION(BlueprintPure, Category="Ball")
	FVector GetBallVelocity() const;

protected:

	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity);

	UFUNCTION()
	void HandleStop(const FHitResult& ImpactResult);

	UFUNCTION()
	void OnRep_InPlay();

	UFUNCTION()
	void OnRep_RallyState();

	void SetMovementActive(bool bActive);
	void ApplyBalance();
	void UpdateShadow();
	void UpdateNetCrossing();
	void UpdateVisualSmoothing(float DeltaSeconds);
	void SnapVisual();
	void DrawDebugTrajectory() const;

	double LastGroundContactTime = -1.0;
	bool bOutOfPlayReported = false;

	FTransform CourtFrame = FTransform::Identity;
	bool bHasCourtFrame = false;
	FVector PreviousCourtLocal = FVector::ZeroVector;

	/** Client: last shot id seen, to snap the visual on a new shot */
	int32 LastSeenShotId = 0;
	bool bSnapNextNetUpdate = false;
};
