#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "JGCharacterRoster.generated.h"

class UJGCharacterData;

/**
 *  선택할 수 있는 선수 목록. 로비 외형, 경기 시작 선수, 선택 화면이 모두 이 에셋 하나를 읽는다.
 *  이 클래스로 DA_JGCharacterRoster를 만들고 Project Settings > Game > Jokgu > Characters에 지정한다.
 *  선수를 늘리거나 줄일 때는 CharacterData 에셋을 만들고 이 목록만 고치면 된다.
 */
UCLASS(BlueprintType)
class UJGCharacterRoster : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** 설정된 로스터 에셋을 반환하고, 지정된 에셋이 없으면 빈 클래스 기본값을 반환한다 */
	static const UJGCharacterRoster* Get();

	/** 목록 순서가 선택 화면 순서다. 첫 번째와 두 번째 선수가 A팀·B팀의 시작 선수다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Roster")
	TArray<TObjectPtr<UJGCharacterData>> Characters;

	/** 저장된 선택이 없거나 목록에 없는 ID일 때 쓰는 선수. 비우면 목록의 첫 선수다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Roster")
	TObjectPtr<UJGCharacterData> DefaultCharacter;

	/** ID로 찾는다. 목록에 없으면 nullptr. */
	UJGCharacterData* Find(FName CharacterId) const;

	/** ID로 찾고, 목록에 없으면 기본 선수를 반환한다 */
	UJGCharacterData* FindOrDefault(FName CharacterId) const;

	UJGCharacterData* GetDefaultCharacter() const;

	/** 목록의 Index번째 선수. 범위 밖이거나 비어 있으면 기본 선수다. */
	UJGCharacterData* GetAtOrDefault(int32 Index) const;
};
