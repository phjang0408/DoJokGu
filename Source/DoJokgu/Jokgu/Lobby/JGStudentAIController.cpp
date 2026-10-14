#include "Jokgu/Lobby/JGStudentAIController.h"
#include "DoJokgu.h"
#include "Jokgu/Lobby/JGStudentCharacter.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

namespace
{
	/** NavMesh가 아직 없을 때(런타임 생성 중 등) 다시 시도하는 간격 */
	constexpr float RetryDelay = 3.0f;

	/** 목표 지점에 이 거리(cm) 안으로 들어가면 도착으로 본다 */
	constexpr float AcceptanceRadius = 60.0f;

	/** 너무 가까운 목표는 다시 고른다 */
	constexpr float MinTravelDistance = 400.0f;
}

AJGStudentAIController::AJGStudentAIController()
{
	bReplicates = false;
}

void AJGStudentAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// 학생들이 한꺼번에 출발하지 않도록 처음 대기 시간을 흩뜨린다
	ScheduleNextMove(FMath::FRandRange(0.5f, 4.0f));
}

void AJGStudentAIController::OnUnPossess()
{
	GetWorldTimerManager().ClearTimer(NextMoveTimer);

	Super::OnUnPossess();
}

void AJGStudentAIController::ScheduleNextMove(float Delay)
{
	GetWorldTimerManager().SetTimer(NextMoveTimer, this, &AJGStudentAIController::MoveToRandomPoint, FMath::Max(Delay, 0.1f), false);
}

void AJGStudentAIController::MoveToRandomPoint()
{
	const AJGStudentCharacter* Student = GetPawn<AJGStudentCharacter>();
	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());

	if (!Student)
	{
		return;
	}

	// 집 주변에서 고르되, 지금 자리와 너무 가까운 곳은 몇 번 다시 고른다
	FNavLocation Target;
	bool bFound = false;
	for (int32 Attempt = 0; NavSystem && Attempt < 4 && !bFound; ++Attempt)
	{
		bFound = NavSystem->GetRandomReachablePointInRadius(Student->GetHomeLocation(), Student->GetWanderRadius(), Target)
			&& FVector::Dist2D(Target.Location, Student->GetActorLocation()) >= MinTravelDistance;
	}

	const EPathFollowingRequestResult::Type Request = bFound
		? MoveToLocation(Target.Location, AcceptanceRadius, true, true, false, false)
		: EPathFollowingRequestResult::Failed;

	if (Request == EPathFollowingRequestResult::Failed)
	{
		// NavMesh는 맵을 연 뒤 만들어지므로 처음 몇 번은 실패할 수 있다. 원인은 처음 한 번만 남긴다.
		if (!bWarnedNoNavigation)
		{
			const TCHAR* Reason = !NavSystem ? TEXT("no navigation system")
				: !NavSystem->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) ? TEXT("no NavMesh yet")
				: !bFound ? TEXT("no reachable point near home")
				: TEXT("move request rejected");
			UE_LOG(LogDoJokgu, Warning, TEXT("Student %s: cannot wander (%s), retrying"), *Student->GetName(), Reason);
			bWarnedNoNavigation = true;
		}

		++FailedAttempts;
		ScheduleNextMove(RetryDelay);
		return;
	}

	if (FailedAttempts > 0)
	{
		UE_LOG(LogDoJokgu, Log, TEXT("Student %s: wandering after %d retries"), *Student->GetName(), FailedAttempts);
		FailedAttempts = 0;
	}

	if (Request == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		ScheduleNextMove(1.0f);
	}
}

void AJGStudentAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);

	AJGStudentCharacter* Student = GetPawn<AJGStudentCharacter>();
	if (!Student)
	{
		return;
	}

	const FVector2D WaitRange = Student->GetWaitTimeRange();
	float Delay = FMath::FRandRange(WaitRange.X, WaitRange.Y);

	// 제대로 도착했을 때만 가끔 감정표현을 하고, 그 길이만큼 더 기다린다
	if (Result.IsSuccess() && FMath::FRand() < Student->GetEmoteChance())
	{
		Delay += Student->PlayRandomEmote();
	}

	ScheduleNextMove(Delay);
}
