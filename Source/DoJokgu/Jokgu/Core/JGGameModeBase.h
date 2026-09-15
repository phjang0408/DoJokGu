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
 *  Server-only match rules: joining, spawning, serve, rally end, scoring and the win check.
 *  Phase flow: WaitingPlayers -> PreparingServe -> Serving -> Rally -> PointEnd -> (PreparingServe | MatchEnd)
 */
UCLASS()
class AJGGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:

	AJGGameModeBase();

	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void StartPlay() override;

	bool IsPracticeMode() const { return bPracticeMode; }

	/** True if the character may attack now. bOutIsServe is set when the attack is the serve. */
	bool CanCharacterAttack(const AJGCharacter* Character, bool& bOutIsServe) const;

	bool CanCharacterSlide(const AJGCharacter* Character) const;

	/** Called by a character after it applied a hit to the ball */
	void NotifyBallHit(AJGCharacter* Hitter, bool bWasServe);

	/** Collects rematch votes on the result screen */
	void HandleRematchRequest(APlayerController* Requester);

	/** Resets scores and starts from the first serve */
	UFUNCTION(BlueprintCallable, Category="Match")
	void StartNewMatch();

protected:

	UPROPERTY(EditDefaultsOnly, Category="Match")
	TSubclassOf<AJGBall> BallClass;

	/** Spawned at the world origin if the level has no court */
	UPROPERTY(EditDefaultsOnly, Category="Match")
	TSubclassOf<AJGCourt> FallbackCourtClass;

	/** Practice: no match flow, ground contacts are only reported on screen. Required for AJGBallLauncher. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Match")
	bool bPracticeMode = false;

	/** Starts a match with a single player, useful for testing serve and scoring alone */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Match")
	bool bAllowSoloMatch = false;

	UPROPERTY(EditDefaultsOnly, Category="Characters")
	TObjectPtr<UJGCharacterData> DefaultCharacterData;

	UPROPERTY(EditDefaultsOnly, Category="Characters")
	TArray<TObjectPtr<UJGCharacterData>> CharacterRoster;

	UPROPERTY(Transient)
	TObjectPtr<AJGCourt> Court;

	UPROPERTY(Transient)
	TObjectPtr<AJGBall> Ball;

	void EnsureCourtAndBall();
	void TryStartMatch();

	void EnterPreparingServe();
	void EnterServing();
	void HandleServeTimeout();
	void FinishPoint();

	void AwardPoint(EJGTeam ScoringTeam, EJGPointReason Reason);
	void EndMatch(EJGMatchEndReason Reason, EJGTeam Winner);

	void SetPhase(EJGMatchPhase Phase, float Duration);
	void SetPhaseTimer(void (AJGGameModeBase::*Callback)(), float Delay);
	void ClearPhaseTimer();

	void ResetPlayersForServe();
	void PlaceBallForServe();

	void HandleBallGroundContact(AJGBall* InBall, const FVector& Location);
	void HandleBallStopped(AJGBall* InBall, const FVector& Location);
	void ApplyContactResult(const FJGContactResult& Result);

	EJGTeam FindFreeTeam() const;
	int32 CountTeamPlayers(const AController* Ignore = nullptr) const;
	bool HasPlayerOnTeam(EJGTeam Team) const;
	AJGCharacter* FindCharacterOnTeam(EJGTeam Team) const;
	UJGCharacterData* FindCharacterData(FName CharacterId) const;
	int32 GetRequiredPlayers() const;

	AJGGameStateBase* GetJGGameState() const;

	FTimerHandle PhaseTimer;

	/** Guarantees a single point per round, whatever the number of contacts */
	bool bPointResolvedThisRound = false;

	static constexpr int32 MaxMatchPlayers = 2;
};
