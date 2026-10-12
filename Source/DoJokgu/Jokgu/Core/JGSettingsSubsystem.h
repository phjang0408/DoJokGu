#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "JGSettingsSubsystem.generated.h"

class UJGProfileSaveGame;
class UWorld;

UENUM(BlueprintType)
enum class EJGVolumeChannel : uint8
{
	Master,
	Effects,
	Music
};

/** 설정 화면의 세 단계. 엔진 Scalability 0(낮음), 1(중간), 2(높음)에 대응한다. */
UENUM(BlueprintType)
enum class EJGGraphicsQuality : uint8
{
	Low,
	Medium,
	High
};

/**
 *  기기별 개인 설정의 단일 창구: 저장 파일(UJGProfileSaveGame)을 한 번 읽어 들고 있고,
 *  볼륨은 사운드 믹스로, 그래픽 품질은 GameUserSettings로 적용한다.
 *  맵이 바뀔 때마다 볼륨을 다시 건다.
 */
UCLASS()
class UJGSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	static UJGSettingsSubsystem* Get(const UObject* WorldContext);

#pragma region 볼륨

	/** 0~1. 바로 소리에 적용한다. bSave면 저장 파일에도 쓴다(슬라이더를 끄는 동안에는 false). */
	UFUNCTION(BlueprintCallable, Category="Jokgu|Settings")
	void SetVolume(EJGVolumeChannel Channel, float Volume, bool bSave = true);

	UFUNCTION(BlueprintPure, Category="Jokgu|Settings")
	float GetVolume(EJGVolumeChannel Channel) const;

#pragma endregion

#pragma region 그래픽

	UFUNCTION(BlueprintCallable, Category="Jokgu|Settings")
	void SetGraphicsQuality(EJGGraphicsQuality Quality);

	/** 사용자 지정 Scalability(-1)는 높음으로 본다 */
	UFUNCTION(BlueprintPure, Category="Jokgu|Settings")
	EJGGraphicsQuality GetGraphicsQuality() const;

#pragma endregion

#pragma region 선수

	UFUNCTION(BlueprintPure, Category="Jokgu|Settings")
	FName GetLastCharacterId() const;

	UFUNCTION(BlueprintCallable, Category="Jokgu|Settings")
	void SetLastCharacterId(FName CharacterId);

#pragma endregion

	/** 지금 값을 저장 파일에 쓴다 */
	UFUNCTION(BlueprintCallable, Category="Jokgu|Settings")
	void SaveProfile();

protected:

	/** 볼륨 세 개를 사운드 믹스 덮어쓰기로 걸고 믹스를 켠다 */
	void ApplyVolumes(UWorld* World) const;

	void HandlePostLoadMap(UWorld* World);

	float& VolumeRef(EJGVolumeChannel Channel) const;

	UPROPERTY(Transient)
	TObjectPtr<UJGProfileSaveGame> Profile;

	FDelegateHandle PostLoadMapHandle;
};
