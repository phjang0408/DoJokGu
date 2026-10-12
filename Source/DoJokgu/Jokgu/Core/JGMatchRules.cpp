#include "Jokgu/Core/JGMatchRules.h"

#pragma region 팀과 코트 구역

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

	// 네트 바로 아래(X = 0)는 B 코트로 친다
	return CourtLocalPoint.X < 0.0 ? EJGCourtZone::CourtA : EJGCourtZone::CourtB;
}

#pragma endregion

#pragma region 랠리 상태

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
	State.bCrossedNet = false;
	State.NetCrossingClearance = 0.0f;
	State.NetCrossingLateral = 0.0f;
	++State.ShotId;
}

bool UJGMatchRules::RegisterNetCrossing(FJGRallyState& State, const FVector& PreviousCourtLocal, const FVector& CurrentCourtLocal, float NetHeight, float BallRadius)
{
	if (State.bCrossedNet || State.LastHitterTeam == EJGTeam::None)
	{
		return false;
	}

	// 받는 쪽이 A면 로컬 -X, B면 +X 코트
	const double ReceiverSign = State.ExpectedReceiverTeam == EJGTeam::A ? -1.0 : 1.0;
	const double Previous = PreviousCourtLocal.X * ReceiverSign;
	const double Current = CurrentCourtLocal.X * ReceiverSign;

	// 받는 쪽 방향 부호로 바꾼 값이 0 이하에서 0 초과로 바뀐 순간만 넘은 것으로 본다
	if (Previous > 0.0 || Current <= 0.0)
	{
		return false;
	}

	// 두 위치 사이를 선형 보간해 X = 0을 지나는 지점을 구한다
	const double Alpha = Previous / (Previous - Current);
	const FVector Crossing = FMath::Lerp(PreviousCourtLocal, CurrentCourtLocal, Alpha);

	State.bCrossedNet = true;
	State.NetCrossingClearance = static_cast<float>(Crossing.Z - BallRadius - NetHeight);
	State.NetCrossingLateral = static_cast<float>(Crossing.Y);
	return true;
}

void UJGMatchRules::ClearRally(FJGRallyState& State)
{
	State.LastHitterTeam = EJGTeam::None;
	State.ExpectedReceiverTeam = EJGTeam::None;
	State.ReceiverBounceCount = 0;
	State.bHasLandedInReceiverCourt = false;
}

#pragma endregion

#pragma region 득점 판정

FJGContactResult UJGMatchRules::EvaluateGroundContact(FJGRallyState& State, EJGCourtZone Zone)
{
	FJGContactResult Result;

	// 랠리 중인 공이 아니다 (서브를 위해 들고 있거나, 이미 득점이 결정됨)
	if (State.LastHitterTeam == EJGTeam::None)
	{
		return Result;
	}

	if (!State.bHasLandedInReceiverCourt)
	{
		// 받는 쪽 코트에 처음 정상 착지: 한 번 튀는 것은 허용한다
		if (Zone == GetTeamCourtZone(State.ExpectedReceiverTeam))
		{
			State.bHasLandedInReceiverCourt = true;
			State.ReceiverBounceCount = 1;
			return Result;
		}

		// 밖에 떨어졌거나 넘어가지 못함: 친 쪽이 실점한다
		Result.bPointScored = true;
		Result.ScoringTeam = State.ExpectedReceiverTeam;
		Result.Reason = Zone == EJGCourtZone::Out ? EJGPointReason::OutOnFirstLanding : EJGPointReason::FailedToCross;
		return Result;
	}

	// 받는 쪽이 받아 넘기기 전에 두 번째 접촉: 위치와 상관없이 받는 쪽이 실점한다
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

	// 받는 쪽 코트에 이미 떨어졌으면 받는 쪽 책임, 아니면 넘기지 못한 친 쪽 책임
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

#pragma endregion

#pragma region 대기 단계

bool UJGMatchRules::IsCharacterSelectOpen(EJGMatchPhase Phase, EJGMatchEndReason EndReason)
{
	// 점수로 끝난 경기는 같은 캐릭터로 재경기하므로 다시 고르지 않는다
	return Phase == EJGMatchPhase::WaitingPlayers
		|| (Phase == EJGMatchPhase::MatchEnd && EndReason == EJGMatchEndReason::PlayerLeft);
}

bool UJGMatchRules::CanStartMatch(int32 TeamPlayers, int32 ReadyTeamPlayers, int32 RequiredPlayers)
{
	return TeamPlayers > 0 && TeamPlayers >= RequiredPlayers && ReadyTeamPlayers >= TeamPlayers;
}

#pragma endregion
