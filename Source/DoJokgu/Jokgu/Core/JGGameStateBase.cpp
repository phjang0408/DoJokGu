#include "Jokgu/Core/JGGameStateBase.h"
#include "Jokgu/Core/JGMatchRules.h"
#include "Jokgu/Data/JGCharacterData.h"
#include "Jokgu/Data/JGCharacterRoster.h"
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

#pragma region 조회

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

	// 클라이언트도 서버 월드 시간 기준으로 계산해서 Host와 같은 남은 시간을 본다
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
		// 코트는 레벨에 배치된 액터라서 클라이언트는 복제 참조가 오기 전에도 직접 찾을 수 있다
		for (TActorIterator<AJGCourt> It(GetWorld()); It; ++It)
		{
			CachedCourt = *It;
			break;
		}
	}

	return CachedCourt.Get();
}

bool AJGGameStateBase::IsCharacterSelectOpen() const
{
	return UJGMatchRules::IsCharacterSelectOpen(Match.Phase, Match.EndReason);
}

const TArray<TObjectPtr<UJGCharacterData>>& AJGGameStateBase::GetCharacterRoster() const
{
	return UJGCharacterRoster::Get()->Characters;
}

UJGCharacterData* AJGGameStateBase::FindRosterCharacter(FName CharacterId) const
{
	return UJGCharacterRoster::Get()->Find(CharacterId);
}

#pragma endregion

#pragma region 서버 전용 변경과 알림

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

#pragma endregion
