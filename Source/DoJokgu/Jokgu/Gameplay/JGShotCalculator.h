#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Jokgu/Core/JGTypes.h"
#include "JGShotCalculator.generated.h"

class AJGCourt;

/**
 *  Turns gestures into attack inputs and attack inputs into ball velocities.
 *  The same functions are used for the on-screen arrow and for the server hit, so both always agree.
 *  Drag shots are never corrected to land inside the court: net faults and outs must stay possible.
 */
UCLASS()
class UJGShotCalculator : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** (drag length - dead zone) / (max length - dead zone), clamped to [0, 1] */
	UFUNCTION(BlueprintPure, Category="Jokgu|Shot")
	static float ComputeDragPower(float DragLength);

	/** Aim angle from a drag in widget space (X right, Y down). Dragging up is straight ahead. */
	UFUNCTION(BlueprintPure, Category="Jokgu|Shot")
	static float ComputeAimAngle(const FVector2D& DragDelta);

	/** Resolves a released touch. Returns false for a long press without movement (cancelled). */
	UFUNCTION(BlueprintPure, Category="Jokgu|Shot")
	static bool ResolveTouchGesture(const FVector2D& DragDelta, float HeldSeconds, FJGAttackInput& OutInput);

	/** Picks the animation slot. Serve always uses the serve motion. */
	UFUNCTION(BlueprintPure, Category="Jokgu|Shot")
	static EJGKickMotion SelectKickMotion(const FJGAttackInput& Input, bool bIsServe);

	/** World direction of an aim angle around the attack forward */
	UFUNCTION(BlueprintPure, Category="Jokgu|Shot")
	static FVector GetAimDirection(const FVector& AttackForward, float AimAngleDeg);

	/** Drag velocity: speed changes continuously with power, direction follows the aim */
	UFUNCTION(BlueprintPure, Category="Jokgu|Shot")
	static FVector ComputeDragVelocity(const FVector& AttackForward, float AimAngleDeg, float Power, float KickPowerMultiplier = 1.0f);

	/** Velocity that reaches Target after FlightTime seconds */
	UFUNCTION(BlueprintPure, Category="Jokgu|Shot")
	static FVector ComputeVelocityToTarget(const FVector& Start, const FVector& Target, float FlightTime, float GravityZ);

	/** Solves a parabola from Start through Via (e.g. above the net) down to Target. Returns false if no downward arc fits. */
	UFUNCTION(BlueprintPure, Category="Jokgu|Shot")
	static bool SolveArcThroughPoint(const FVector& Start, const FVector& Via, const FVector& Target, float GravityZ, FVector& OutVelocity);

	/** Tap: short drop just over the net, in line with the hitter's lateral position */
	static FVector ComputeTapVelocity(const AJGCourt& Court, EJGTeam HitterTeam, const FVector& BallLocation, float GravityZ);

	/** Slide save: safe high return toward the opponent court center */
	static FVector ComputeSlideReturnVelocity(const AJGCourt& Court, EJGTeam HitterTeam, const FVector& BallLocation, float GravityZ);

	/** Full attack resolution used by the server hit */
	static FVector ComputeAttackVelocity(const AJGCourt& Court, EJGTeam HitterTeam, const FVector& BallLocation, const FJGAttackInput& Input, float KickPowerMultiplier, float AimDeviationDeg, float GravityZ);
};
