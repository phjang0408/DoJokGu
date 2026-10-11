#include "Jokgu/Core/JGPlayerState.h"
#include "Net/UnrealNetwork.h"

#pragma region 복제

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

	// 로비에서 고른 값은 경기 맵으로 seamless travel 해도 유지한다.
	// 팀과 재경기 요청은 경기 맵의 게임 모드가 새로 정한다.
	if (AJGPlayerState* Target = Cast<AJGPlayerState>(PlayerState))
	{
		Target->CharacterId = CharacterId;
		Target->bReady = bReady;
	}
}

#pragma endregion

#pragma region 서버 전용 설정

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

#pragma endregion
