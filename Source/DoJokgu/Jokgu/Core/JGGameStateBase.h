#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Jokgu/Core/JGTypes.h"
#include "JGGameStateBase.generated.h"

class AJGBall;
class AJGCourt;
class AJGGameStateBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FJGMatchStateChangedSignature, AJGGameStateBase*, GameState);

/**
 *  Match state everyone must see: scores, phase, serving team, round and phase end time.
 *  The host decides (game mode) and this class distributes. The host broadcasts directly after a change and clients
 *  broadcast from the RepNotify, so UI always goes through the same OnMatchStateChanged path.
 */
UCLASS()
class AJGGameStateBase : public AGameStateBase
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintAssignable, Category="Match")
	FJGMatchStateChangedSignature OnMatchStateChanged;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="Match")
	FJGMatchSnapshot GetMatch() const { return Match; }

	UFUNCTION(BlueprintPure, Category="Match")
	EJGMatchPhase GetPhase() const { return Match.Phase; }

	UFUNCTION(BlueprintPure, Category="Match")
	EJGTeam GetServingTeam() const { return Match.ServingTeam; }

	UFUNCTION(BlueprintPure, Category="Match")
	int32 GetScore(EJGTeam Team) const;

	/** Seconds left in the current phase, 0 if the phase has no time limit */
	UFUNCTION(BlueprintPure, Category="Match")
	float GetPhaseRemainingSeconds() const;

	UFUNCTION(BlueprintPure, Category="Match")
	AJGBall* GetBall() const { return Ball; }

	UFUNCTION(BlueprintPure, Category="Match")
	AJGCourt* GetCourt() const;

	/** Server: registers the level court and the spawned ball */
	void SetCourtAndBall(AJGCourt* InCourt, AJGBall* InBall);

	/** Server: mutable access. Call CommitMatchChanges() once after editing. */
	FJGMatchSnapshot& EditMatch() { return Match; }

	/** Server: pushes the change to clients and notifies local listeners */
	void CommitMatchChanges();

protected:

	UPROPERTY(ReplicatedUsing=OnRep_Match)
	FJGMatchSnapshot Match;

	UPROPERTY(Replicated)
	TObjectPtr<AJGBall> Ball;

	UPROPERTY(Replicated)
	TObjectPtr<AJGCourt> Court;

	UFUNCTION()
	void OnRep_Match();

	/** Court found locally while the replicated reference has not arrived yet */
	mutable TWeakObjectPtr<AJGCourt> CachedCourt;
};
