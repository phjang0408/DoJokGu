#include "Misc/AutomationTest.h"
#include "Jokgu/Core/JGMatchRules.h"
#include "Jokgu/Data/JGBalanceData.h"
#include "Jokgu/Gameplay/JGShotCalculator.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJGRulesDeuceTest, "Jokgu.Rules.Deuce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FJGRulesDeuceTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("6:6 continues"), UJGMatchRules::GetMatchWinner(6, 6, 7, 2) == EJGTeam::None);
	TestTrue(TEXT("7:6 continues"), UJGMatchRules::GetMatchWinner(7, 6, 7, 2) == EJGTeam::None);
	TestTrue(TEXT("8:6 A wins"), UJGMatchRules::GetMatchWinner(8, 6, 7, 2) == EJGTeam::A);
	TestTrue(TEXT("7:5 A wins"), UJGMatchRules::GetMatchWinner(7, 5, 7, 2) == EJGTeam::A);
	TestTrue(TEXT("5:7 B wins"), UJGMatchRules::GetMatchWinner(5, 7, 7, 2) == EJGTeam::B);
	TestTrue(TEXT("10:9 continues"), UJGMatchRules::GetMatchWinner(10, 9, 7, 2) == EJGTeam::None);
	TestTrue(TEXT("9:11 B wins"), UJGMatchRules::GetMatchWinner(9, 11, 7, 2) == EJGTeam::B);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJGRulesCourtZoneTest, "Jokgu.Rules.CourtZone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FJGRulesCourtZoneTest::RunTest(const FString& Parameters)
{
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJGRulesGroundContactTest, "Jokgu.Rules.GroundContact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FJGRulesGroundContactTest::RunTest(const FString& Parameters)
{
	// first landing out: hitter loses
	{
		FJGRallyState State;
		UJGMatchRules::RegisterHit(State, EJGTeam::A);
		const FJGContactResult Result = UJGMatchRules::EvaluateGroundContact(State, EJGCourtZone::Out);
		TestTrue(TEXT("out: point scored"), Result.bPointScored);
		TestTrue(TEXT("out: receiver scores"), Result.ScoringTeam == EJGTeam::B);
		TestTrue(TEXT("out: reason"), Result.Reason == EJGPointReason::OutOnFirstLanding);
	}

	// lands on own side (net fault): hitter loses
	{
		FJGRallyState State;
		UJGMatchRules::RegisterHit(State, EJGTeam::A);
		const FJGContactResult Result = UJGMatchRules::EvaluateGroundContact(State, EJGCourtZone::CourtA);
		TestTrue(TEXT("own side: receiver scores"), Result.bPointScored && Result.ScoringTeam == EJGTeam::B);
		TestTrue(TEXT("own side: reason"), Result.Reason == EJGPointReason::FailedToCross);
	}

	// in, then out without a return: receiver loses on the second contact
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

	// legal return resets receiver info and prevents a double hit by the same team
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

	// cleared rally never scores again
	{
		FJGRallyState State;
		UJGMatchRules::RegisterHit(State, EJGTeam::A);
		UJGMatchRules::ClearRally(State);
		TestFalse(TEXT("cleared: no point"), UJGMatchRules::EvaluateGroundContact(State, EJGCourtZone::Out).bPointScored);
		TestFalse(TEXT("cleared: not hittable"), UJGMatchRules::CanTeamHit(State, EJGTeam::B));
	}

	// ball stopped before reaching the receiver court (e.g. resting on the net)
	{
		FJGRallyState State;
		UJGMatchRules::RegisterHit(State, EJGTeam::B);
		const FJGContactResult Result = UJGMatchRules::EvaluateBallStopped(State);
		TestTrue(TEXT("stopped: receiver scores"), Result.bPointScored && Result.ScoringTeam == EJGTeam::A);
		TestTrue(TEXT("stopped: reason"), Result.Reason == EJGPointReason::FailedToCross);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJGShotCalculatorTest, "Jokgu.Shot.Calculator",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FJGShotCalculatorTest::RunTest(const FString& Parameters)
{
	const UJGBalanceData* Balance = UJGBalanceData::Get();

	// drag power
	TestEqual(TEXT("dead zone gives 0"), UJGShotCalculator::ComputeDragPower(Balance->DragDeadZone), 0.0f);
	TestEqual(TEXT("max length gives 1"), UJGShotCalculator::ComputeDragPower(Balance->DragMaxLength), 1.0f);
	TestEqual(TEXT("beyond max is clamped"), UJGShotCalculator::ComputeDragPower(Balance->DragMaxLength * 3.0f), 1.0f);

	// 45% and 65% share a motion slot but must produce different ball speeds
	const FVector Forward = FVector::ForwardVector;
	const FVector V45 = UJGShotCalculator::ComputeDragVelocity(Forward, 0.0f, 0.45f);
	const FVector V65 = UJGShotCalculator::ComputeDragVelocity(Forward, 0.0f, 0.65f);
	TestTrue(TEXT("power changes speed continuously"), V65.Size2D() > V45.Size2D() + 1.0);

	// aim angle is clamped and positive to the right
	const FVector Right = UJGShotCalculator::ComputeDragVelocity(Forward, 90.0f, 0.5f);
	TestTrue(TEXT("aim right goes +Y"), Right.Y > 0.0);
	const double Angle = FMath::RadiansToDegrees(FMath::Atan2(Right.Y, Right.X));
	TestTrue(TEXT("aim clamped to limit"), FMath::IsNearlyEqual(Angle, static_cast<double>(Balance->AimAngleLimitDeg), 0.01));

	// gestures
	FJGAttackInput Input;
	TestTrue(TEXT("short press is a tap"), UJGShotCalculator::ResolveTouchGesture(FVector2D(1.0, 1.0), 0.05f, Input) && Input.Type == EJGAttackType::Tap);
	TestFalse(TEXT("long press without move is cancelled"), UJGShotCalculator::ResolveTouchGesture(FVector2D::ZeroVector, 1.0f, Input));
	TestTrue(TEXT("drag up is a straight full drag"),
		UJGShotCalculator::ResolveTouchGesture(FVector2D(0.0, -500.0), 0.3f, Input)
		&& Input.Type == EJGAttackType::Drag && FMath::IsNearlyZero(Input.AimAngleDeg) && FMath::IsNearlyEqual(Input.Power, 1.0f));

	// arc through the net clearance point
	const FVector Start(-500.0, 0.0, 50.0);
	const FVector Via(0.0, 0.0, 153.0);
	const FVector Target(150.0, 0.0, 13.0);
	const double GravityZ = -980.0;

	FVector Velocity;
	TestTrue(TEXT("arc solvable"), UJGShotCalculator::SolveArcThroughPoint(Start, Via, Target, GravityZ, Velocity));

	auto HeightAtX = [&](double X)
	{
		const double Time = (X - Start.X) / Velocity.X;
		return Start.Z + Velocity.Z * Time + 0.5 * GravityZ * Time * Time;
	};

	TestTrue(TEXT("arc passes the via point"), FMath::IsNearlyEqual(HeightAtX(Via.X), Via.Z, 0.5));
	TestTrue(TEXT("arc reaches the target"), FMath::IsNearlyEqual(HeightAtX(Target.X), Target.Z, 0.5));

	return true;
}

#endif
