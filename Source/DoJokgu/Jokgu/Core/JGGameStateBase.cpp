#include "Jokgu/Core/JGGameStateBase.h"
#include "Jokgu/Gameplay/JGBall.h"
#include "Jokgu/Gameplay/JGCourt.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"

void AJGGameStateBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AJGGameStateBase, Match);
	DOREPLIFETIME(AJGGameStateBase, Ball);
	DOREPLIFETIME(AJGGameStateBase, Court);
}

int32 AJGGameStateBase::GetScore(EJGTeam Team) const
{
	switch (Team)
	{
	case EJGTeam::A: return Match.ScoreA;
	case EJGTeam::B: return Match.ScoreB;
	default: return 0;
	}
}

float AJGGameStateBase::GetPhaseRemainingSeconds() const
{
	if (Match.PhaseEndServerTime <= 0.0)
	{
		return 0.0f;
	}

	return FMath::Max(0.0f, static_cast<float>(Match.PhaseEndServerTime - GetServerWorldTimeSeconds()));
}

AJGCourt* AJGGameStateBase::GetCourt() const
{
	if (Court)
	{
		return Court;
	}

	if (!CachedCourt.IsValid())
	{
		// the court is a level actor, so clients can find it before the replicated reference arrives
		for (TActorIterator<AJGCourt> It(GetWorld()); It; ++It)
		{
			CachedCourt = *It;
			break;
		}
	}

	return CachedCourt.Get();
}

void AJGGameStateBase::SetCourtAndBall(AJGCourt* InCourt, AJGBall* InBall)
{
	Court = InCourt;
	Ball = InBall;
	ForceNetUpdate();
}

void AJGGameStateBase::CommitMatchChanges()
{
	ForceNetUpdate();
	OnMatchStateChanged.Broadcast(this);
}

void AJGGameStateBase::OnRep_Match()
{
	OnMatchStateChanged.Broadcast(this);
}
