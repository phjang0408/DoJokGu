#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Jokgu/Core/JGTypes.h"
#include "JGGameModeBase.generated.h"

class AJGBall;
class AJGCharacter;
class AJGCourt;
class AJGGameStateBase;
class UJGCharacterData;

/**
 *  서버 전용 경기 규칙: 입장, 스폰, 서브, 랠리 종료, 득점, 승리 판정.
 *  단계 흐름: WaitingPlayers -> PreparingServe -> Serving -> Rally -> PointEnd -> (PreparingServe | MatchEnd)
 */
UCLASS()
class AJGGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:

	AJGGameModeBase();

#pragma region 엔진 오버라이드

	/** 정원(MaxMatchPlayers)이 찼으면 접속을 거절한다 */
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	/** 빈 팀과 시작 캐릭터를 정한 뒤 경기 시작을 시도한다 */
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	/** 팀과 서브 여부에 맞는 코트 시작 위치에 스폰하고 캐릭터 데이터를 적용한다 */
	virtual void RestartPlayer(AController* NewPlayer) override;
	/** 경기 중 이탈하면 경기를 끝낸다 */
	virtual void Logout(AController* Exiting) override;
	virtual void StartPlay() override;

#pragma endregion

#pragma region 캐릭터 요청 처리

	bool IsPracticeMode() const { return bPracticeMode; }

	/** 캐릭터가 지금 공격할 수 있으면 true. 그 공격이 서브면 bOutIsServe를 켠다. */
	bool CanCharacterAttack(const AJGCharacter* Character, bool& bOutIsServe) const;

	/** 랠리 중(연습 모드는 항상)에만 슬라이딩을 허용한다 */
	bool CanCharacterSlide(const AJGCharacter* Character) const;

	/** 캐릭터가 공에 타격을 적용한 뒤 호출한다 */
	void NotifyBallHit(AJGCharacter* Hitter, bool bWasServe);

	/** 결과 화면에서 재경기 투표를 모은다 */
	void HandleRematchRequest(APlayerController* Requester);

	/** 플레이어를 로스터 캐릭터로 바꾼다. 로스터에 없는 ID는 무시한다. */
	void HandleCharacterSelect(APlayerController* Requester, FName CharacterId);

	/** 점수를 초기화하고 첫 서브부터 시작한다 */
	UFUNCTION(BlueprintCallable, Category="Match")
	void StartNewMatch();

#pragma endregion

protected:

#pragma region 설정

	UPROPERTY(EditDefaultsOnly, Category="Match")
	TSubclassOf<AJGBall> BallClass;

	/** 레벨에 코트가 없으면 월드 원점에 스폰한다 */
	UPROPERTY(EditDefaultsOnly, Category="Match")
	TSubclassOf<AJGCourt> FallbackCourtClass;

	/** 연습: 경기 흐름 없이 지면 접촉 결과만 화면에 보여 준다. AJGBallLauncher를 쓰려면 켜야 한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Match")
	bool bPracticeMode = false;

	/** 플레이어 한 명으로도 경기를 시작한다. 혼자 서브와 득점을 시험할 때 쓴다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Match")
	bool bAllowSoloMatch = false;

	/** 로스터에서 캐릭터를 찾지 못했을 때 쓰는 데이터 */
	UPROPERTY(EditDefaultsOnly, Category="Characters")
	TObjectPtr<UJGCharacterData> DefaultCharacterData;

	/** 선택 가능한 캐릭터. 선택 화면이 생기기 전까지 A팀은 0번, B팀은 1번으로 시작한다. */
	UPROPERTY(EditDefaultsOnly, Category="Characters")
	TArray<TObjectPtr<UJGCharacterData>> CharacterRoster;

#pragma endregion

#pragma region 코트와 공

	UPROPERTY(Transient)
	TObjectPtr<AJGCourt> Court;

	UPROPERTY(Transient)
	TObjectPtr<AJGBall> Ball;

	/** 레벨의 코트를 찾고(없으면 스폰) 공을 스폰해 게임 스테이트에 등록한다. 여러 번 불러도 안전하다. */
	void EnsureCourtAndBall();

	/** 필요한 인원이 모였고 대기 중이면 새 경기를 시작한다 */
	void TryStartMatch();

#pragma endregion

#pragma region 단계 전환

	void EnterPreparingServe();
	void EnterServing();
	void HandleServeTimeout();

	/** 득점 후 대기가 끝나면 승리 판정을 하고 다음 서브나 경기 종료로 넘어간다 */
	void FinishPoint();

	void AwardPoint(EJGTeam ScoringTeam, EJGPointReason Reason);
	void EndMatch(EJGMatchEndReason Reason, EJGTeam Winner);

	/** 단계와 종료 시각을 바꾸고 바로 복제한다. Duration이 0이면 시간 제한이 없다. */
	void SetPhase(EJGMatchPhase Phase, float Duration);

	/** Delay 뒤에 Callback을 한 번 부른다. Delay가 0 이하면 즉시 부른다. */
	void SetPhaseTimer(void (AJGGameModeBase::*Callback)(), float Delay);
	void ClearPhaseTimer();

#pragma endregion

#pragma region 서브 배치

	void ResetPlayersForServe();
	void PlaceBallForServe();

#pragma endregion

#pragma region 공 판정

	void HandleBallGroundContact(AJGBall* InBall, const FVector& Location);
	void HandleBallStopped(AJGBall* InBall, const FVector& Location);

	/** 판정 결과를 경기 모드에서는 득점으로, 연습 모드에서는 화면 메시지로 반영한다 */
	void ApplyContactResult(const FJGContactResult& Result);

#pragma endregion

#pragma region 조회 도우미

	/** 아무도 쓰지 않는 팀. A부터 채우고 둘 다 차면 None이다. */
	EJGTeam FindFreeTeam() const;

	/** 팀이 정해진 플레이어 수. Ignore는 세지 않는다(나가는 중인 플레이어용). */
	int32 CountTeamPlayers(const AController* Ignore = nullptr) const;

	bool HasPlayerOnTeam(EJGTeam Team) const;
	AJGCharacter* FindCharacterOnTeam(EJGTeam Team) const;

	/** 로스터에서 ID로 찾는다. 없으면 DefaultCharacterData를 반환한다. */
	UJGCharacterData* FindCharacterData(FName CharacterId) const;

	UJGCharacterData* GetStartingCharacterData(EJGTeam Team) const;

	/** 경기 시작에 필요한 인원. bAllowSoloMatch면 1명이다. */
	int32 GetRequiredPlayers() const;

	AJGGameStateBase* GetJGGameState() const;

#pragma endregion

	FTimerHandle PhaseTimer;

	/** 접촉이 몇 번 일어나든 라운드마다 득점이 한 번만 나오게 한다 */
	bool bPointResolvedThisRound = false;

	static constexpr int32 MaxMatchPlayers = 2;
};
