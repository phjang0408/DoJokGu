#include "Jokgu/Core/JGPlayerState.h"
#include "Net/UnrealNetwork.h"

void AJGPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AJGPlayerState, Team);
	DOREPLIFETIME(AJGPlayerState, CharacterId);
	DOREPLIFETIME(AJGPlayerState, bReady);
	DOREPLIFETIME(AJGPlayerState, bWantsRematch);
}

void AJGPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);

	// lobby choices survive seamless travel into the match map
	if (AJGPlayerState* Target = Cast<AJGPlayerState>(PlayerState))
	{
		Target->CharacterId = CharacterId;
		Target->bReady = bReady;
	}
}

void AJGPlayerState::SetTeam(EJGTeam NewTeam)
{
	Team = NewTeam;
	ForceNetUpdate();
}

void AJGPlayerState::SetCharacterId(FName NewCharacterId)
{
	CharacterId = NewCharacterId;
	ForceNetUpdate();
}

void AJGPlayerState::SetReady(bool bNewReady)
{
	bReady = bNewReady;
	ForceNetUpdate();
}

void AJGPlayerState::SetWantsRematch(bool bNewWantsRematch)
{
	bWantsRematch = bNewWantsRematch;
	ForceNetUpdate();
}
