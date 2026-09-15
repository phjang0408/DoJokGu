#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Jokgu/Core/JGTypes.h"
#include "JGMatchRules.generated.h"

/**
 *  Pure rule functions (no world access) so they can be covered by automation tests.
 *  The game mode feeds them with court classification and ball state.
 */
UCLASS()
class UJGMatchRules : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintPure, Category="Jokgu|Rules")
	static EJGTeam GetOpposingTeam(EJGTeam Team);

	UFUNCTION(BlueprintPure, Category="Jokgu|Rules")
	static EJGCourtZone GetTeamCourtZone(EJGTeam Team);

	/**
	 *  Classifies a point given in court local space (origin under the net center, +X toward team B).
	 *  The half extents describe the outer edge of the lines, so a contact on the line is in.
	 */
	UFUNCTION(BlueprintPure, Category="Jokgu|Rules")
	static EJGCourtZone ClassifyCourtPoint(const FVector2D& CourtLocalPoint, float HalfLength, float HalfWidth);

	/** True if the team is the expected receiver of a live ball (a team can never hit its own shot twice in a row) */
	UFUNCTION(BlueprintPure, Category="Jokgu|Rules")
	static bool CanTeamHit(const FJGRallyState& State, EJGTeam Team);

	/** Updates the rally state for a legal hit: new shot id, new receiver, bounce info reset */
	UFUNCTION(BlueprintCallable, Category="Jokgu|Rules")
	static void RegisterHit(UPARAM(ref) FJGRallyState& State, EJGTeam HitterTeam);

	/** Clears the hitter so the ball is no longer judged or hittable */
	UFUNCTION(BlueprintCallable, Category="Jokgu|Rules")
	static void ClearRally(UPARAM(ref) FJGRallyState& State);

	/** Evaluates a ground contact and updates the bounce info in place */
	UFUNCTION(BlueprintCallable, Category="Jokgu|Rules")
	static FJGContactResult EvaluateGroundContact(UPARAM(ref) FJGRallyState& State, EJGCourtZone Zone);

	/** Evaluates a ball that stopped moving without a decisive ground contact (e.g. resting on the net) */
	UFUNCTION(BlueprintPure, Category="Jokgu|Rules")
	static FJGContactResult EvaluateBallStopped(const FJGRallyState& State);

	/** Returns the winner, or None if the match continues. Deuce: reach the target score with the required margin. */
	UFUNCTION(BlueprintPure, Category="Jokgu|Rules")
	static EJGTeam GetMatchWinner(int32 ScoreA, int32 ScoreB, int32 TargetScore, int32 WinMargin);
};
