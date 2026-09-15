#include "Jokgu/Core/JGMatchRules.h"

EJGTeam UJGMatchRules::GetOpposingTeam(EJGTeam Team)
{
	switch (Team)
	{
	case EJGTeam::A: return EJGTeam::B;
	case EJGTeam::B: return EJGTeam::A;
	default: return EJGTeam::None;
	}
}

EJGCourtZone UJGMatchRules::GetTeamCourtZone(EJGTeam Team)
{
	switch (Team)
	{
	case EJGTeam::A: return EJGCourtZone::CourtA;
	case EJGTeam::B: return EJGCourtZone::CourtB;
	default: return EJGCourtZone::Out;
	}
}

EJGCourtZone UJGMatchRules::ClassifyCourtPoint(const FVector2D& CourtLocalPoint, float HalfLength, float HalfWidth)
{
	if (FMath::Abs(CourtLocalPoint.X) > HalfLength || FMath::Abs(CourtLocalPoint.Y) > HalfWidth)
	{
		return EJGCourtZone::Out;
	}

	return CourtLocalPoint.X < 0.0 ? EJGCourtZone::CourtA : EJGCourtZone::CourtB;
}

bool UJGMatchRules::CanTeamHit(const FJGRallyState& State, EJGTeam Team)
{
	return Team != EJGTeam::None
		&& State.LastHitterTeam != EJGTeam::None
		&& State.ExpectedReceiverTeam == Team;
}

void UJGMatchRules::RegisterHit(FJGRallyState& State, EJGTeam HitterTeam)
{
	State.LastHitterTeam = HitterTeam;
	State.ExpectedReceiverTeam = GetOpposingTeam(HitterTeam);
	State.ReceiverBounceCount = 0;
	State.bHasLandedInReceiverCourt = false;
	++State.ShotId;
}

void UJGMatchRules::ClearRally(FJGRallyState& State)
{
	State.LastHitterTeam = EJGTeam::None;
	State.ExpectedReceiverTeam = EJGTeam::None;
	State.ReceiverBounceCount = 0;
	State.bHasLandedInReceiverCourt = false;
}

FJGContactResult UJGMatchRules::EvaluateGroundContact(FJGRallyState& State, EJGCourtZone Zone)
{
	FJGContactResult Result;

	// ball is not in a rally (held for a serve, or the point is already decided)
	if (State.LastHitterTeam == EJGTeam::None)
	{
		return Result;
	}

	if (!State.bHasLandedInReceiverCourt)
	{
		// first valid landing in the receiver court: one bounce is allowed
		if (Zone == GetTeamCourtZone(State.ExpectedReceiverTeam))
		{
			State.bHasLandedInReceiverCourt = true;
			State.ReceiverBounceCount = 1;
			return Result;
		}

		// landed out, or never crossed: the hitter loses the point
		Result.bPointScored = true;
		Result.ScoringTeam = State.ExpectedReceiverTeam;
		Result.Reason = Zone == EJGCourtZone::Out ? EJGPointReason::OutOnFirstLanding : EJGPointReason::FailedToCross;
		return Result;
	}

	// second contact before the receiver returned the ball, wherever it is
	++State.ReceiverBounceCount;
	Result.bPointScored = true;
	Result.ScoringTeam = State.LastHitterTeam;
	Result.Reason = EJGPointReason::NotReturned;
	return Result;
}

FJGContactResult UJGMatchRules::EvaluateBallStopped(const FJGRallyState& State)
{
	FJGContactResult Result;

	if (State.LastHitterTeam == EJGTeam::None)
	{
		return Result;
	}

	Result.bPointScored = true;

	if (State.bHasLandedInReceiverCourt)
	{
		Result.ScoringTeam = State.LastHitterTeam;
		Result.Reason = EJGPointReason::NotReturned;
	}
	else
	{
		Result.ScoringTeam = State.ExpectedReceiverTeam;
		Result.Reason = EJGPointReason::FailedToCross;
	}

	return Result;
}

EJGTeam UJGMatchRules::GetMatchWinner(int32 ScoreA, int32 ScoreB, int32 TargetScore, int32 WinMargin)
{
	if (ScoreA >= TargetScore && ScoreA - ScoreB >= WinMargin)
	{
		return EJGTeam::A;
	}

	if (ScoreB >= TargetScore && ScoreB - ScoreA >= WinMargin)
	{
		return EJGTeam::B;
	}

	return EJGTeam::None;
}
