#pragma once

#include "CoreMinimal.h"
#include "JGTypes.generated.h"

/** Object channel used by the ball. Registered as "JGBall" in DefaultEngine.ini with a default response of Ignore,
 *  so only surfaces that explicitly block it (court floor, net) can bounce the ball. */
#define ECC_JGBall ECC_GameTraceChannel1

namespace JGTags
{
	/** Component tag for surfaces that count as a ground contact for the rally rules */
	inline FName Ground() { static const FName Tag(TEXT("JGGround")); return Tag; }

	/** Component tag for the net collision */
	inline FName Net() { static const FName Tag(TEXT("JGNet")); return Tag; }
}

/** Team / court side. Team A owns the -X half of the court and attacks toward +X. */
UENUM(BlueprintType)
enum class EJGTeam : uint8
{
	None,
	A,
	B
};

/** Match flow. Each phase defines which inputs are accepted and how it ends. */
UENUM(BlueprintType)
enum class EJGMatchPhase : uint8
{
	WaitingPlayers,
	PreparingServe,
	Serving,
	Rally,
	PointEnd,
	MatchEnd
};

/** Where a ground contact happened, in court terms */
UENUM(BlueprintType)
enum class EJGCourtZone : uint8
{
	Out,
	CourtA,
	CourtB
};

/** Attack gesture confirmed on touch release */
UENUM(BlueprintType)
enum class EJGAttackType : uint8
{
	None,
	Tap,
	Drag
};

/** Animation slot to play for a kick. Only picks the motion; ball speed always comes from the continuous power value. */
UENUM(BlueprintType)
enum class EJGKickMotion : uint8
{
	Tap,
	Weak,
	Medium,
	Strong,
	Serve,
	Slide
};

UENUM(BlueprintType)
enum class EJGSlideState : uint8
{
	Ready,
	Sliding,
	Recovery
};

UENUM(BlueprintType)
enum class EJGPointReason : uint8
{
	None,
	/** First landing was outside the court: the hitter loses the point */
	OutOnFirstLanding,
	/** Ball did not reach the receiver court (net / own side) before landing or stopping: the hitter loses the point */
	FailedToCross,
	/** Ball touched the ground again after landing in the receiver court: the receiver loses the point */
	NotReturned,
	/** Server did not serve within the time limit */
	ServeTimeout
};

UENUM(BlueprintType)
enum class EJGMatchEndReason : uint8
{
	None,
	ScoreReached,
	PlayerLeft
};

/** Attack request sent from the owning client to the server */
USTRUCT(BlueprintType)
struct FJGAttackInput
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
	EJGAttackType Type = EJGAttackType::None;

	/** Horizontal aim in degrees relative to the own-court-to-opponent-court direction. Positive is to the player's right. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
	float AimAngleDeg = 0.0f;

	/** Normalized drag power in [0, 1]. Ignored for taps. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
	float Power = 0.0f;

	/** Client side sequence number, useful for logs */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
	int32 InputId = 0;

	bool IsValid() const { return Type != EJGAttackType::None; }
};

/** Everything the server needs to judge a ground contact. Owned by the ball. */
USTRUCT(BlueprintType)
struct FJGRallyState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rally")
	EJGTeam LastHitterTeam = EJGTeam::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rally")
	EJGTeam ExpectedReceiverTeam = EJGTeam::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rally")
	int32 ReceiverBounceCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rally")
	bool bHasLandedInReceiverCourt = false;

	/** Incremented on every legal hit */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rally")
	int32 ShotId = 0;
};

/** Result of evaluating a ground contact or a stopped ball */
USTRUCT(BlueprintType)
struct FJGContactResult
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rally")
	bool bPointScored = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rally")
	EJGTeam ScoringTeam = EJGTeam::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rally")
	EJGPointReason Reason = EJGPointReason::None;
};

/** Match state that every client must agree on. Replicated as a single property so it always arrives consistent. */
USTRUCT(BlueprintType)
struct FJGMatchSnapshot
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Match")
	EJGMatchPhase Phase = EJGMatchPhase::WaitingPlayers;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Match")
	int32 ScoreA = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Match")
	int32 ScoreB = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Match")
	EJGTeam ServingTeam = EJGTeam::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Match")
	int32 RoundNumber = 0;

	/** Server world time when the current phase ends. 0 if the phase has no time limit. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Match")
	double PhaseEndServerTime = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Match")
	EJGTeam LastScoringTeam = EJGTeam::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Match")
	EJGPointReason LastPointReason = EJGPointReason::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Match")
	EJGTeam WinnerTeam = EJGTeam::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Match")
	EJGMatchEndReason EndReason = EJGMatchEndReason::None;
};
