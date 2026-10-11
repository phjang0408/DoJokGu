#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Jokgu/Core/JGTypes.h"
#include "JGPlayerState.generated.h"

/**
 *  상대도 알아야 하는 플레이어 정보: 팀, 선택한 캐릭터, 로비 준비 여부, 재경기 요청 여부.
 *  값은 서버에서만 바꾼다.
 */
UCLASS()
class AJGPlayerState : public APlayerState
{
	GENERATED_BODY()

public:

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void CopyProperties(APlayerState* PlayerState) override;

#pragma region 조회

	UFUNCTION(BlueprintPure, Category="Player")
	EJGTeam GetTeam() const { return Team; }

	UFUNCTION(BlueprintPure, Category="Player")
	FName GetCharacterId() const { return CharacterId; }

	UFUNCTION(BlueprintPure, Category="Player")
	bool IsReady() const { return bReady; }

	UFUNCTION(BlueprintPure, Category="Player")
	bool WantsRematch() const { return bWantsRematch; }

#pragma endregion

#pragma region 서버 전용 설정

	// 모두 값을 바꾼 뒤 ForceNetUpdate로 바로 복제한다

	void SetTeam(EJGTeam NewTeam);
	void SetCharacterId(FName NewCharacterId);
	void SetReady(bool bNewReady);
	void SetWantsRematch(bool bNewWantsRematch);

#pragma endregion

protected:

#pragma region 복제 상태

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Player")
	EJGTeam Team = EJGTeam::None;

	/** 로스터의 캐릭터 ID (UJGCharacterData::CharacterId) */
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Player")
	FName CharacterId;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Player")
	bool bReady = false;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Player")
	bool bWantsRematch = false;

#pragma endregion
};
