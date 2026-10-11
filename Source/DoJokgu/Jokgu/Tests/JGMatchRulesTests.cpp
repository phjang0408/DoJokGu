#include "Misc/AutomationTest.h"
#include "Jokgu/Core/JGMatchRules.h"
#include "Jokgu/Data/JGBalanceData.h"
#include "Jokgu/Gameplay/JGShotCalculator.h"

// 월드 없이 도는 순수 규칙·샷 계산 테스트. Session Frontend나 "Automation RunTests Jokgu"로 실행한다.

#if WITH_DEV_AUTOMATION_TESTS

#pragma region 듀스

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJGRulesDeuceTest, "Jokgu.Rules.Deuce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FJGRulesDeuceTest::RunTest(const FString& Parameters)
{
	// 목표 7점, 2점 차
	TestTrue(TEXT("6:6 continues"), UJGMatchRules::GetMatchWinner(6, 6, 7, 2) == EJGTeam::None);
	TestTrue(TEXT("7:6 continues"), UJGMatchRules::GetMatchWinner(7, 6, 7, 2) == EJGTeam::None);
	TestTrue(TEXT("8:6 A wins"), UJGMatchRules::GetMatchWinner(8, 6, 7, 2) == EJGTeam::A);
	TestTrue(TEXT("7:5 A wins"), UJGMatchRules::GetMatchWinner(7, 5, 7, 2) == EJGTeam::A);
	TestTrue(TEXT("5:7 B wins"), UJGMatchRules::GetMatchWinner(5, 7, 7, 2) == EJGTeam::B);
	TestTrue(TEXT("10:9 continues"), UJGMatchRules::GetMatchWinner(10, 9, 7, 2) == EJGTeam::None);
	TestTrue(TEXT("9:11 B wins"), UJGMatchRules::GetMatchWinner(9, 11, 7, 2) == EJGTeam::B);
	return true;
}

#pragma endregion

#pragma region 코트 구역

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJGRulesCourtZoneTest, "Jokgu.Rules.CourtZone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FJGRulesCourtZoneTest::RunTest(const FString& Parameters)
{
	// 12 x 8 m 코트
	const float HalfLength = 600.0f;
	const float HalfWidth = 400.0f;

	TestTrue(TEXT("corner on the line is in (A)"), UJGMatchRules::ClassifyCourtPoint(FVector2D(-600.0, 400.0), HalfLength, HalfWidth) == EJGCourtZone::CourtA);
	TestTrue(TEXT("corner on the line is in (B)"), UJGMatchRules::ClassifyCourtPoint(FVector2D(600.0, -400.0), HalfLength, HalfWidth) == EJGCourtZone::CourtB);
	TestTrue(TEXT("just past the base line is out"), UJGMatchRules::ClassifyCourtPoint(FVector2D(-600.5, 0.0), HalfLength, HalfWidth) == EJGCourtZone::Out);
	TestTrue(TEXT("just past the side line is out"), UJGMatchRules::ClassifyCourtPoint(FVector2D(100.0, 400.5), HalfLength, HalfWidth) == EJGCourtZone::Out);
	TestTrue(TEXT("negative X is A"), UJGMatchRules::ClassifyCourtPoint(FVector2D(-1.0, 0.0), HalfLength, HalfWidth) == EJGCourtZone::CourtA);
	TestTrue(TEXT("positive X is B"), UJGMatchRules::ClassifyCourtPoint(FVector2D(1.0, 0.0), HalfLength, HalfWidth) == EJGCourtZone::CourtB);
	return true;
}

#pragma endregion

#pragma region 지면 접촉

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJGRulesGroundContactTest, "Jokgu.Rules.GroundContact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FJGRulesGroundContactTest::RunTest(const FString& Parameters)
{
	// 첫 착지가 밖: 친 쪽이 실점
	{
		FJGRallyState State;
		UJGMatchRules::RegisterHit(State, EJGTeam::A);
		const FJGContactResult Result = UJGMatchRules::EvaluateGroundContact(State, EJGCourtZone::Out);
		TestTrue(TEXT("out: point scored"), Result.bPointScored);
		TestTrue(TEXT("out: receiver scores"), Result.ScoringTeam == EJGTeam::B);
		TestTrue(TEXT("out: reason"), Result.Reason == EJGPointReason::OutOnFirstLanding);
	}

	// 자기 코트에 떨어짐(네트 실수): 친 쪽이 실점
	{
		FJGRallyState State;
		UJGMatchRules::RegisterHit(State, EJGTeam::A);
		const FJGContactResult Result = UJGMatchRules::EvaluateGroundContact(State, EJGCourtZone::CourtA);
		TestTrue(TEXT("own side: receiver scores"), Result.bPointScored && Result.ScoringTeam == EJGTeam::B);
		TestTrue(TEXT("own side: reason"), Result.Reason == EJGPointReason::FailedToCross);
	}

	// 인으로 떨어진 뒤 받아 넘기지 못하고 밖에 닿음: 두 번째 접촉에서 받는 쪽이 실점
	{
		FJGRallyState State;
		UJGMatchRules::RegisterHit(State, EJGTeam::A);
		const FJGContactResult First = UJGMatchRules::EvaluateGroundContact(State, EJGCourtZone::CourtB);
		TestFalse(TEXT("in: no point on first bounce"), First.bPointScored);
		TestEqual(TEXT("in: bounce count"), State.ReceiverBounceCount, 1);

		const FJGContactResult Second = UJGMatchRules::EvaluateGroundContact(State, EJGCourtZone::Out);
		TestTrue(TEXT("second contact: hitter scores"), Second.bPointScored && Second.ScoringTeam == EJGTeam::A);
		TestTrue(TEXT("second contact: reason"), Second.Reason == EJGPointReason::NotReturned);
	}

	// 합법 반환은 받는 쪽 정보를 초기화하고, 같은 팀이 연속으로 치는 것을 막는다
	{
		FJGRallyState State;
		UJGMatchRules::RegisterHit(State, EJGTeam::A);
		UJGMatchRules::EvaluateGroundContact(State, EJGCourtZone::CourtB);
		const int32 ShotBefore = State.ShotId;

		TestTrue(TEXT("B can hit A's shot"), UJGMatchRules::CanTeamHit(State, EJGTeam::B));
		TestFalse(TEXT("A cannot hit its own shot"), UJGMatchRules::CanTeamHit(State, EJGTeam::A));

		UJGMatchRules::RegisterHit(State, EJGTeam::B);
		TestTrue(TEXT("new shot id"), State.ShotId == ShotBefore + 1);
		TestTrue(TEXT("receiver switched"), State.ExpectedReceiverTeam == EJGTeam::A);
		TestFalse(TEXT("landing reset"), State.bHasLandedInReceiverCourt);
		TestEqual(TEXT("bounce reset"), State.ReceiverBounceCount, 0);
	}

	// 지운 랠리는 다시 득점하지 않는다
	{
		FJGRallyState State;
		UJGMatchRules::RegisterHit(State, EJGTeam::A);
		UJGMatchRules::ClearRally(State);
		TestFalse(TEXT("cleared: no point"), UJGMatchRules::EvaluateGroundContact(State, EJGCourtZone::Out).bPointScored);
		TestFalse(TEXT("cleared: not hittable"), UJGMatchRules::CanTeamHit(State, EJGTeam::B));
	}

	// 받는 쪽 코트에 닿기 전에 멈춘 공 (예: 네트 위에 얹힘)
	{
		FJGRallyState State;
		UJGMatchRules::RegisterHit(State, EJGTeam::B);
		const FJGContactResult Result = UJGMatchRules::EvaluateBallStopped(State);
		TestTrue(TEXT("stopped: receiver scores"), Result.bPointScored && Result.ScoringTeam == EJGTeam::A);
		TestTrue(TEXT("stopped: reason"), Result.Reason == EJGPointReason::FailedToCross);
	}

	return true;
}

#pragma endregion

#pragma region 네트 통과

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJGRulesNetCrossingTest, "Jokgu.Rules.NetCrossing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FJGRulesNetCrossingTest::RunTest(const FString& Parameters)
{
	const float NetHeight = 105.0f;
	const float BallRadius = 13.0f;

	// A는 +X로 공격한다: 넘는 지점은 네트 평면 위에서 보간한다
	// (X -10 -> 30 구간의 1/4 지점이므로 Y = 30, Z = 170)
	{
		FJGRallyState State;
		UJGMatchRules::RegisterHit(State, EJGTeam::A);
		TestFalse(TEXT("no crossing on own side"), UJGMatchRules::RegisterNetCrossing(State, FVector(-50.0, 0.0, 150.0), FVector(-10.0, 0.0, 160.0), NetHeight, BallRadius));
		TestTrue(TEXT("crossing recorded"), UJGMatchRules::RegisterNetCrossing(State, FVector(-10.0, 20.0, 160.0), FVector(30.0, 60.0, 200.0), NetHeight, BallRadius));
		TestTrue(TEXT("crossed flag"), State.bCrossedNet);
		TestTrue(TEXT("clearance at the plane"), FMath::IsNearlyEqual(State.NetCrossingClearance, 170.0f - BallRadius - NetHeight, 0.01f));
		TestTrue(TEXT("lateral at the plane"), FMath::IsNearlyEqual(State.NetCrossingLateral, 30.0f, 0.01f));
		TestFalse(TEXT("recorded once per shot"), UJGMatchRules::RegisterNetCrossing(State, FVector(-10.0, 0.0, 160.0), FVector(30.0, 0.0, 200.0), NetHeight, BallRadius));
	}

	// B는 -X로 공격한다: 자기 코트 쪽으로 돌아가는 움직임은 넘은 것이 아니다
	{
		FJGRallyState State;
		UJGMatchRules::RegisterHit(State, EJGTeam::B);
		TestFalse(TEXT("wrong direction"), UJGMatchRules::RegisterNetCrossing(State, FVector(-10.0, 0.0, 100.0), FVector(10.0, 0.0, 100.0), NetHeight, BallRadius));
		TestTrue(TEXT("B crossing"), UJGMatchRules::RegisterNetCrossing(State, FVector(10.0, 0.0, 110.0), FVector(-10.0, 0.0, 110.0), NetHeight, BallRadius));
		TestTrue(TEXT("touching the net gives negative clearance"), State.NetCrossingClearance < 0.0f);
	}

	// 새 타격은 기록을 초기화하고, 지운 랠리는 아무것도 기록하지 않는다
	{
		FJGRallyState State;
		UJGMatchRules::RegisterHit(State, EJGTeam::A);
		UJGMatchRules::RegisterNetCrossing(State, FVector(-10.0, 0.0, 200.0), FVector(10.0, 0.0, 200.0), NetHeight, BallRadius);
		UJGMatchRules::RegisterHit(State, EJGTeam::B);
		TestFalse(TEXT("reset on new hit"), State.bCrossedNet);

		UJGMatchRules::ClearRally(State);
		TestFalse(TEXT("cleared: nothing recorded"), UJGMatchRules::RegisterNetCrossing(State, FVector(10.0, 0.0, 200.0), FVector(-10.0, 0.0, 200.0), NetHeight, BallRadius));
	}

	return true;
}

#pragma endregion

#pragma region 샷 계산

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJGShotCalculatorTest, "Jokgu.Shot.Calculator",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FJGShotCalculatorTest::RunTest(const FString& Parameters)
{
	const UJGBalanceData* Balance = UJGBalanceData::Get();

	// 드래그 파워
	TestEqual(TEXT("dead zone gives 0"), UJGShotCalculator::ComputeDragPower(Balance->DragDeadZone), 0.0f);
	TestEqual(TEXT("max length gives 1"), UJGShotCalculator::ComputeDragPower(Balance->DragMaxLength), 1.0f);
	TestEqual(TEXT("beyond max is clamped"), UJGShotCalculator::ComputeDragPower(Balance->DragMaxLength * 3.0f), 1.0f);

	// 45%와 65%는 같은 동작 슬롯(중)이지만 공 속도는 달라야 한다
	const FVector Forward = FVector::ForwardVector;
	const FVector V45 = UJGShotCalculator::ComputeDragVelocity(Forward, 0.0f, 0.45f);
	const FVector V65 = UJGShotCalculator::ComputeDragVelocity(Forward, 0.0f, 0.65f);
	TestTrue(TEXT("power changes speed continuously"), V65.Size2D() > V45.Size2D() + 1.0);

	// 조준 각도는 한계로 잘리고, 양수가 오른쪽이다
	const FVector Right = UJGShotCalculator::ComputeDragVelocity(Forward, 90.0f, 0.5f);
	TestTrue(TEXT("aim right goes +Y"), Right.Y > 0.0);
	const double Angle = FMath::RadiansToDegrees(FMath::Atan2(Right.Y, Right.X));
	TestTrue(TEXT("aim clamped to limit"), FMath::IsNearlyEqual(Angle, static_cast<double>(Balance->AimAngleLimitDeg), 0.01));

	// 제스처 해석
	FJGAttackInput Input;
	TestTrue(TEXT("short press is a tap"), UJGShotCalculator::ResolveTouchGesture(FVector2D(1.0, 1.0), 0.05f, Input) && Input.Type == EJGAttackType::Tap);
	TestFalse(TEXT("long press without move is cancelled"), UJGShotCalculator::ResolveTouchGesture(FVector2D::ZeroVector, 1.0f, Input));
	TestTrue(TEXT("drag up is a straight full drag"),
		UJGShotCalculator::ResolveTouchGesture(FVector2D(0.0, -500.0), 0.3f, Input)
		&& Input.Type == EJGAttackType::Drag && FMath::IsNearlyZero(Input.AimAngleDeg) && FMath::IsNearlyEqual(Input.Power, 1.0f));

	// 네트 위 여유 지점을 지나는 포물선
	const FVector Start(-500.0, 0.0, 50.0);
	const FVector Via(0.0, 0.0, 153.0);
	const FVector Target(150.0, 0.0, 13.0);
	const double GravityZ = -980.0;

	FVector Velocity;
	TestTrue(TEXT("arc solvable"), UJGShotCalculator::SolveArcThroughPoint(Start, Via, Target, GravityZ, Velocity));

	// 구한 속도로 X 위치에서의 높이를 다시 계산해 경유점과 목표를 지나는지 확인한다
	auto HeightAtX = [&](double X)
	{
		const double Time = (X - Start.X) / Velocity.X;
		return Start.Z + Velocity.Z * Time + 0.5 * GravityZ * Time * Time;
	};

	TestTrue(TEXT("arc passes the via point"), FMath::IsNearlyEqual(HeightAtX(Via.X), Via.Z, 0.5));
	TestTrue(TEXT("arc reaches the target"), FMath::IsNearlyEqual(HeightAtX(Target.X), Target.Z, 0.5));

	return true;
}

#pragma endregion

#endif
