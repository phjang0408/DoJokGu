#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Jokgu/Core/JGTypes.h"
#include "JGPlayerState.generated.h"

/**
 *  Player information the opponent also needs: team, selected character, lobby ready and rematch flags.
 *  Values are only changed by the server.
 */
UCLASS()
class AJGPlayerState : public APlayerState
{
	GENERATED_BODY()

public:

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void CopyProperties(APlayerState* PlayerState) override;

	UFUNCTION(BlueprintPure, Category="Player")
	EJGTeam GetTeam() const { return Team; }

	UFUNCTION(BlueprintPure, Category="Player")
	FName GetCharacterId() const { return CharacterId; }

	UFUNCTION(BlueprintPure, Category="Player")
	bool IsReady() const { return bReady; }

	UFUNCTION(BlueprintPure, Category="Player")
	bool WantsRematch() const { return bWantsRematch; }

	void SetTeam(EJGTeam NewTeam);
	void SetCharacterId(FName NewCharacterId);
	void SetReady(bool bNewReady);
	void SetWantsRematch(bool bNewWantsRematch);

protected:

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Player")
	EJGTeam Team = EJGTeam::None;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Player")
	FName CharacterId;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Player")
	bool bReady = false;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Player")
	bool bWantsRematch = false;
};
