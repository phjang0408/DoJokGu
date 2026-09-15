#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Curves/CurveFloat.h"
#include "JGBalanceData.generated.h"

/**
 *  All tuning numbers in one place (cm, seconds). Values are initial experiments, not official jokgu dimensions.
 *  Create DA_Balance_Default from this class and assign it in Project Settings > Game > Jokgu.
 */
UCLASS(BlueprintType)
class UJGBalanceData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	UJGBalanceData();

	/** Returns the configured balance asset, or the class defaults if none is assigned */
	static const UJGBalanceData* Get();

	/** Blueprint access to the configured balance asset */
	UFUNCTION(BlueprintPure, Category="Jokgu|Balance", DisplayName="Get Jokgu Balance Data")
	static UJGBalanceData* GetBalanceData();

	/** Evaluates a normalized curve. An empty curve behaves as identity. */
	static float EvaluateCurve(const FRuntimeFloatCurve& Curve, float InTime);

	/** Multiplier for a 1-5 character stat (move speed, kick power, reach) */
	UFUNCTION(BlueprintPure, Category="Jokgu|Balance")
	float GetStatMultiplier(int32 Stat) const;

	/** Maximum random aim deviation in degrees for an accuracy stat */
	UFUNCTION(BlueprintPure, Category="Jokgu|Balance")
	float GetAccuracyDeviationDeg(int32 AccuracyStat) const;

	// ----- Match -----

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Match", meta=(ClampMin=1))
	int32 TargetScore = 7;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Match", meta=(ClampMin=1))
	int32 WinMargin = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Match", meta=(ClampMin=0, Units="s"))
	float ServePrepareTime = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Match", meta=(ClampMin=0, Units="s"))
	float ServeTimeLimit = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Match", meta=(ClampMin=0, Units="s"))
	float PointEndTime = 1.0f;

	// ----- Court -----

	/** Full court length along X, measured at the outer edge of the lines */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Court", meta=(ClampMin=100, Units="cm"))
	float CourtLength = 1200.0f;

	/** Full court width along Y, measured at the outer edge of the lines */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Court", meta=(ClampMin=100, Units="cm"))
	float CourtWidth = 800.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Court", meta=(ClampMin=10, Units="cm"))
	float NetHeight = 105.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Court", meta=(ClampMin=1, Units="cm"))
	float NetThickness = 6.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Court", meta=(ClampMin=1, Units="cm"))
	float CourtLineWidth = 5.0f;

	/** Extra defensive space outside the lines where players may move */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Court", meta=(ClampMin=0, Units="cm"))
	float PlayerAreaMargin = 100.0f;

	/** Distance from the net to the serving spot, inside the own court */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Court", meta=(ClampMin=0, Units="cm"))
	float ServeSpotDistanceFromNet = 500.0f;

	/** Distance from the net to the receiving player's start spot */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Court", meta=(ClampMin=0, Units="cm"))
	float ReceiveSpotDistanceFromNet = 350.0f;

	/** Height above the court floor at which players are placed */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Court", meta=(ClampMin=0, Units="cm"))
	float PlayerSpawnHeight = 100.0f;

	// ----- Ball -----

	/** Collision radius used for judgement. The visual mesh may differ. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ball", meta=(ClampMin=1, Units="cm"))
	float BallRadius = 13.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ball", meta=(ClampMin=0))
	float BallGravityScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ball", meta=(ClampMin=0, ClampMax=1))
	float BallBounciness = 0.55f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ball", meta=(ClampMin=0, ClampMax=1))
	float BallFriction = 0.3f;

	/** Ball is held this far in front of the server's feet */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ball", meta=(Units="cm"))
	float ServeBallForwardOffset = 70.0f;

	/** Ball center height above the server's feet */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ball", meta=(Units="cm"))
	float ServeBallHeight = 60.0f;

	/** Below this world Z (relative to the court) the ball counts as a ground contact out of the court */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ball", meta=(Units="cm"))
	float OutOfPlayZ = -200.0f;

	// ----- Movement -----

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement", meta=(ClampMin=0, Units="CentimetersPerSecond"))
	float BaseMoveSpeed = 450.0f;

	// ----- Touch input -----

	/** A release within this time and within the drag dead zone is a tap */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input", meta=(ClampMin=0, Units="s"))
	float TapMaxDuration = 0.18f;

	/** Drag distance in DPI scaled widget units that is ignored */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input", meta=(ClampMin=0))
	float DragDeadZone = 12.0f;

	/** Drag distance in DPI scaled widget units that reaches full power */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input", meta=(ClampMin=1))
	float DragMaxLength = 140.0f;

	/** Left/right aim limit relative to the attack forward direction */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input", meta=(ClampMin=0, ClampMax=89, Units="deg"))
	float AimAngleLimitDeg = 35.0f;

	/** How long a confirmed attack waits for the ball to enter reach */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input", meta=(ClampMin=0, Units="s"))
	float AttackInputBufferTime = 0.15f;

	/** Delay after a hit before the same player can hit again */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input", meta=(ClampMin=0, Units="s"))
	float AttackRecoveryTime = 0.25f;

	/** Power below this plays the weak kick motion (motion only, never speed) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input", meta=(ClampMin=0, ClampMax=1))
	float WeakMotionMaxPower = 0.33f;

	/** Power at or above this plays the strong kick motion (motion only, never speed) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input", meta=(ClampMin=0, ClampMax=1))
	float StrongMotionMinPower = 0.67f;

	// ----- Hit -----

	/** Horizontal distance from the character to the ball center */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hit", meta=(ClampMin=0, Units="cm"))
	float ReachHorizontal = 120.0f;

	/** Ball center height above the feet */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hit", meta=(Units="cm"))
	float ReachMinHeight = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hit", meta=(Units="cm"))
	float ReachMaxHeight = 130.0f;

	// ----- Shots -----

	/** Tap: the ball lands this far behind the net on the opponent side */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot|Tap", meta=(ClampMin=0, Units="cm"))
	float TapTargetDepth = 150.0f;

	/** Tap: the arc passes this far above the net top */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot|Tap", meta=(ClampMin=0, Units="cm"))
	float TapNetClearance = 35.0f;

	/** Tap: lateral target is kept this far inside the side lines */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot|Tap", meta=(ClampMin=0, Units="cm"))
	float TapLateralInset = 60.0f;

	/** Tap: flight time used when the arc cannot be solved (e.g. ball right at the net) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot|Tap", meta=(ClampMin=0.1, Units="s"))
	float TapFallbackFlightTime = 0.9f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot|Drag", meta=(ClampMin=0, Units="CentimetersPerSecond"))
	float DragHorizontalSpeedAtZero = 350.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot|Drag", meta=(ClampMin=0, Units="CentimetersPerSecond"))
	float DragHorizontalSpeedAtFull = 1100.0f;

	/** Maps power [0,1] to the blend between the horizontal speeds above */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot|Drag")
	FRuntimeFloatCurve DragHorizontalSpeedCurve;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot|Drag", meta=(Units="CentimetersPerSecond"))
	float DragVerticalSpeedAtZero = 520.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot|Drag", meta=(Units="CentimetersPerSecond"))
	float DragVerticalSpeedAtFull = 380.0f;

	/** Maps power [0,1] to the blend between the vertical speeds above */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot|Drag")
	FRuntimeFloatCurve DragVerticalSpeedCurve;

	/** Slide save: the ball goes to this distance behind the net on the opponent side */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot|Slide", meta=(ClampMin=0, Units="cm"))
	float SlideReturnTargetDepth = 300.0f;

	/** Slide save: high and slow, long flight time */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot|Slide", meta=(ClampMin=0.1, Units="s"))
	float SlideReturnFlightTime = 1.6f;

	// ----- Slide -----

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Slide", meta=(ClampMin=0, Units="cm"))
	float SlideDistance = 250.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Slide", meta=(ClampMin=0.05, Units="s"))
	float SlideActiveTime = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Slide", meta=(ClampMin=0, Units="s"))
	float SlideRecoveryTime = 0.6f;

	/** Reach while sliding only. Restored when recovery starts. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Slide", meta=(ClampMin=0, Units="cm"))
	float SlideReachHorizontal = 170.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Slide", meta=(Units="cm"))
	float SlideReachMaxHeight = 80.0f;

	// ----- Camera -----

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera", meta=(ClampMin=-89, ClampMax=0, Units="deg"))
	float CameraPitch = -60.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera", meta=(ClampMin=100, Units="cm"))
	float CameraDistance = 1800.0f;

	/** Camera focus point distance from the net, on the player's own side */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera", meta=(Units="cm"))
	float CameraFocusDistanceFromNet = 150.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera", meta=(ClampMin=5, ClampMax=170, Units="deg"))
	float CameraFieldOfView = 70.0f;

	/** Lateral movement inside this band does not move the camera */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera", meta=(ClampMin=0, Units="cm"))
	float CameraLateralDeadZone = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera", meta=(ClampMin=0, ClampMax=1))
	float CameraLateralFollowRatio = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera", meta=(ClampMin=0, Units="cm"))
	float CameraLateralMaxOffset = 200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera", meta=(ClampMin=0))
	float CameraFollowInterpSpeed = 4.0f;

	// ----- Character stats -----

	/** Stat (1-5) to multiplier for move speed, kick power and reach. 3 = 100%. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats")
	FRuntimeFloatCurve StatMultiplierCurve;

	/** Accuracy stat (1-5) to max random aim deviation in degrees. Starts at 0 to validate the controls first. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats")
	FRuntimeFloatCurve AccuracyDeviationCurve;
};
