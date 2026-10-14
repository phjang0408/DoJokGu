#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "JGStudentAIController.generated.h"

/**
 *  AI 학생의 행동: 집(배치 위치) 주변 NavMesh 위 임의 지점으로 걸어간다 → 멈춘다 → 가끔 감정표현 → 기다린다 → 반복.
 *  비헤이비어 트리 없이 타이머 하나로 돈다. NavMesh가 없거나 지점을 못 찾으면 잠시 뒤 다시 시도한다.
 */
UCLASS()
class AJGStudentAIController : public AAIController
{
	GENERATED_BODY()

public:

	AJGStudentAIController();

	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

protected:

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

private:

	FTimerHandle NextMoveTimer;

	/** NavMesh가 없어 이동하지 못한 것을 한 번만 로그에 남긴다 */
	bool bWarnedNoNavigation = false;

	/** 이동을 시작하지 못한 연속 횟수. 처음 성공할 때 로그에 남기고 0으로 돌린다. */
	int32 FailedAttempts = 0;

	void MoveToRandomPoint();
	void ScheduleNextMove(float Delay);
};
