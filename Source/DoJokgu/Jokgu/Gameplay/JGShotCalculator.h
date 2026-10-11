#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Jokgu/Core/JGTypes.h"
#include "JGShotCalculator.generated.h"

class AJGCourt;

/**
 *  제스처를 공격 입력으로, 공격 입력을 공 속도로 바꾼다.
 *  화면의 조준 화살표와 서버 타격이 같은 함수를 써서 둘이 언제나 일치한다.
 *  드래그 샷은 코트 안에 떨어지도록 보정하지 않는다: 네트 실수와 아웃이 가능해야 한다.
 */
UCLASS()
class UJGShotCalculator : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

#pragma region 입력 해석

	/** (드래그 길이 - 데드존) / (최대 길이 - 데드존)을 [0, 1]로 자른 값 */
	UFUNCTION(BlueprintPure, Category="Jokgu|Shot")
	static float ComputeDragPower(float DragLength);

	/** 위젯 좌표(X 오른쪽, Y 아래)의 드래그에서 조준 각도를 구한다. 위로 끌면 정면이다. */
	UFUNCTION(BlueprintPure, Category="Jokgu|Shot")
	static float ComputeAimAngle(const FVector2D& DragDelta);

	/** 손을 뗀 터치를 해석한다. 움직이지 않고 길게 누른 경우(취소)는 false를 반환한다. */
	UFUNCTION(BlueprintPure, Category="Jokgu|Shot")
	static bool ResolveTouchGesture(const FVector2D& DragDelta, float HeldSeconds, FJGAttackInput& OutInput);

	/** 애니메이션 슬롯을 고른다. 서브는 언제나 서브 동작이다. */
	UFUNCTION(BlueprintPure, Category="Jokgu|Shot")
	static EJGKickMotion SelectKickMotion(const FJGAttackInput& Input, bool bIsServe);

#pragma endregion

#pragma region 궤적 계산

	/** 공격 정면 방향을 조준 각도만큼 돌린 수평 월드 방향 */
	UFUNCTION(BlueprintPure, Category="Jokgu|Shot")
	static FVector GetAimDirection(const FVector& AttackForward, float AimAngleDeg);

	/** 드래그 속도: 파워에 따라 속도가 연속으로 바뀌고 방향은 조준을 따른다 */
	UFUNCTION(BlueprintPure, Category="Jokgu|Shot")
	static FVector ComputeDragVelocity(const FVector& AttackForward, float AimAngleDeg, float Power, float KickPowerMultiplier = 1.0f);

	/** FlightTime초 뒤에 Target에 닿는 속도 */
	UFUNCTION(BlueprintPure, Category="Jokgu|Shot")
	static FVector ComputeVelocityToTarget(const FVector& Start, const FVector& Target, float FlightTime, float GravityZ);

	/** Start에서 Via(예: 네트 위)를 지나 Target으로 내려가는 포물선을 푼다. 조건에 맞는 포물선이 없으면 false를 반환한다. */
	UFUNCTION(BlueprintPure, Category="Jokgu|Shot")
	static bool SolveArcThroughPoint(const FVector& Start, const FVector& Via, const FVector& Target, float GravityZ, FVector& OutVelocity);

#pragma endregion

#pragma region 샷별 속도

	/** 탭: 네트를 살짝 넘겨 떨어뜨리는 짧은 공. 친 사람의 좌우 위치를 그대로 따른다. */
	static FVector ComputeTapVelocity(const AJGCourt& Court, EJGTeam HitterTeam, const FVector& BallLocation, float GravityZ);

	/** 슬라이딩 수비: 상대 코트 가운데로 높게 넘기는 안전한 반환 */
	static FVector ComputeSlideReturnVelocity(const AJGCourt& Court, EJGTeam HitterTeam, const FVector& BallLocation, float GravityZ);

	/** 서버 타격에서 쓰는 전체 공격 계산 */
	static FVector ComputeAttackVelocity(const AJGCourt& Court, EJGTeam HitterTeam, const FVector& BallLocation, const FJGAttackInput& Input, float KickPowerMultiplier, float AimDeviationDeg, float GravityZ);

#pragma endregion
};
