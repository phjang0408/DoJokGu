#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Jokgu/Core/JGTypes.h"
#include "JGMatchRules.generated.h"

/**
 *  월드에 접근하지 않는 순수 규칙 함수 모음. 자동화 테스트로 검증할 수 있게 분리했다.
 *  게임 모드가 코트 구역 분류 결과와 공 상태를 넣어 호출한다.
 */
UCLASS()
class UJGMatchRules : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

#pragma region 팀과 코트 구역

	UFUNCTION(BlueprintPure, Category="Jokgu|Rules")
	static EJGTeam GetOpposingTeam(EJGTeam Team);

	UFUNCTION(BlueprintPure, Category="Jokgu|Rules")
	static EJGCourtZone GetTeamCourtZone(EJGTeam Team);

	/**
	 *  코트 로컬 좌표(원점은 네트 중심 아래, +X는 B팀 쪽)로 주어진 점을 분류한다.
	 *  반 길이·반 폭은 선의 바깥 가장자리 기준이라 선에 닿으면 인이다.
	 */
	UFUNCTION(BlueprintPure, Category="Jokgu|Rules")
	static EJGCourtZone ClassifyCourtPoint(const FVector2D& CourtLocalPoint, float HalfLength, float HalfWidth);

#pragma endregion

#pragma region 랠리 상태

	/** 이 팀이 살아 있는 공을 받아야 하는 팀이면 true (같은 팀은 자기 샷을 연속으로 칠 수 없다) */
	UFUNCTION(BlueprintPure, Category="Jokgu|Rules")
	static bool CanTeamHit(const FJGRallyState& State, EJGTeam Team);

	/** 합법 타격을 반영한다: 새 샷 번호, 새 받는 팀, 바운드 정보 초기화 */
	UFUNCTION(BlueprintCallable, Category="Jokgu|Rules")
	static void RegisterHit(UPARAM(ref) FJGRallyState& State, EJGTeam HitterTeam);

	/** 친 팀을 지워서 공이 더 이상 판정되지도, 칠 수도 없게 한다 */
	UFUNCTION(BlueprintCallable, Category="Jokgu|Rules")
	static void ClearRally(UPARAM(ref) FJGRallyState& State);

	/**
	 *  코트 로컬 좌표의 두 공 위치 사이에서 받는 쪽 코트 방향으로 네트 평면(코트 로컬 X = 0)을
	 *  처음 넘은 순간을 기록한다. 이번 호출에서 기록했으면 true를 반환한다.
	 */
	UFUNCTION(BlueprintCallable, Category="Jokgu|Rules")
	static bool RegisterNetCrossing(UPARAM(ref) FJGRallyState& State, const FVector& PreviousCourtLocal, const FVector& CurrentCourtLocal, float NetHeight, float BallRadius);

#pragma endregion

#pragma region 득점 판정

	/** 지면 접촉을 판정하고 바운드 정보를 그 자리에서 갱신한다 */
	UFUNCTION(BlueprintCallable, Category="Jokgu|Rules")
	static FJGContactResult EvaluateGroundContact(UPARAM(ref) FJGRallyState& State, EJGCourtZone Zone);

	/** 결정적인 지면 접촉 없이 멈춘 공을 판정한다 (예: 네트 위에 얹힌 공) */
	UFUNCTION(BlueprintPure, Category="Jokgu|Rules")
	static FJGContactResult EvaluateBallStopped(const FJGRallyState& State);

	/** 승자를 반환하고, 경기가 계속되면 None을 반환한다. 듀스: 목표 점수에 도달하고 필요한 점수 차를 벌려야 한다. */
	UFUNCTION(BlueprintPure, Category="Jokgu|Rules")
	static EJGTeam GetMatchWinner(int32 ScoreA, int32 ScoreB, int32 TargetScore, int32 WinMargin);

#pragma endregion

#pragma region 대기 단계

	/** 캐릭터를 고르고 준비할 수 있는 단계면 true: 대기 중, 또는 상대가 나가서 끝난 경기 */
	UFUNCTION(BlueprintPure, Category="Jokgu|Rules")
	static bool IsCharacterSelectOpen(EJGMatchPhase Phase, EJGMatchEndReason EndReason);

	/** 팀에 든 인원이 필요한 수 이상이고 그 모두가 준비했으면 true */
	UFUNCTION(BlueprintPure, Category="Jokgu|Rules")
	static bool CanStartMatch(int32 TeamPlayers, int32 ReadyTeamPlayers, int32 RequiredPlayers);

#pragma endregion
};
