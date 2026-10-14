#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Jokgu/Core/JGTypes.h"
#include "JGGameStateBase.generated.h"

class AJGBall;
class AJGCourt;
class AJGGameStateBase;
class UJGCharacterData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FJGMatchStateChangedSignature, AJGGameStateBase*, GameState);

/**
 *  모두가 봐야 하는 경기 상태: 점수, 단계, 서브 팀, 라운드, 단계 종료 시각.
 *  Host(게임 모드)가 결정하고 이 클래스가 나눠 준다. Host는 변경 직후 직접, 클라이언트는 RepNotify에서
 *  브로드캐스트하므로 UI는 언제나 같은 OnMatchStateChanged 경로로 갱신된다.
 */
UCLASS()
class AJGGameStateBase : public AGameStateBase
{
	GENERATED_BODY()

public:

	/** 경기 상태가 바뀔 때마다 Host와 클라이언트 모두에서 호출된다 */
	UPROPERTY(BlueprintAssignable, Category="Match")
	FJGMatchStateChangedSignature OnMatchStateChanged;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma region 조회

	UFUNCTION(BlueprintPure, Category="Match")
	FJGMatchSnapshot GetMatch() const { return Match; }

	UFUNCTION(BlueprintPure, Category="Match")
	EJGMatchPhase GetPhase() const { return Match.Phase; }

	UFUNCTION(BlueprintPure, Category="Match")
	EJGTeam GetServingTeam() const { return Match.ServingTeam; }

	UFUNCTION(BlueprintPure, Category="Match")
	int32 GetScore(EJGTeam Team) const;

	/** 현재 단계의 남은 시간(초). 시간 제한이 없는 단계면 0이다. */
	UFUNCTION(BlueprintPure, Category="Match")
	float GetPhaseRemainingSeconds() const;

	UFUNCTION(BlueprintPure, Category="Match")
	AJGBall* GetBall() const { return Ball; }

	UFUNCTION(BlueprintPure, Category="Match")
	AJGCourt* GetCourt() const;

	/** 지금 캐릭터를 고르고 준비할 수 있으면 true (UJGMatchRules::IsCharacterSelectOpen) */
	UFUNCTION(BlueprintPure, Category="Match")
	bool IsCharacterSelectOpen() const;

	/** 선택 화면이 보여 줄 로스터(UJGCharacterRoster). 프로젝트 설정 값이라 서버와 클라이언트가 같으므로 복제하지 않는다. */
	const TArray<TObjectPtr<UJGCharacterData>>& GetCharacterRoster() const;

	/** 로스터에서 ID로 찾는다. 없으면 nullptr. */
	UJGCharacterData* FindRosterCharacter(FName CharacterId) const;

#pragma endregion

#pragma region 서버 전용 변경

	/** 서버: 레벨의 코트와 스폰한 공을 등록한다 */
	void SetCourtAndBall(AJGCourt* InCourt, AJGBall* InBall);

	/** 서버: 수정용 접근. 수정을 마친 뒤 CommitMatchChanges()를 한 번 호출한다. */
	FJGMatchSnapshot& EditMatch() { return Match; }

	/** 서버: 변경을 클라이언트로 보내고 로컬 리스너에 알린다 */
	void CommitMatchChanges();

#pragma endregion

protected:

#pragma region 복제 상태

	UPROPERTY(ReplicatedUsing=OnRep_Match)
	FJGMatchSnapshot Match;

	UPROPERTY(Replicated)
	TObjectPtr<AJGBall> Ball;

	UPROPERTY(Replicated)
	TObjectPtr<AJGCourt> Court;

	UFUNCTION()
	void OnRep_Match();

	/** 복제 참조가 아직 도착하지 않았을 때 로컬에서 찾아 둔 코트 */
	mutable TWeakObjectPtr<AJGCourt> CachedCourt;

#pragma endregion
};
