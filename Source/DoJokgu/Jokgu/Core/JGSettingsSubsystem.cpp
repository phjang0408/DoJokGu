#include "Jokgu/Core/JGSettingsSubsystem.h"
#include "Jokgu/Data/JGGameSettings.h"
#include "Jokgu/Data/JGProfileSaveGame.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "UObject/UObjectGlobals.h"
#include "DoJokgu.h"

#pragma region 초기화

void UJGSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Profile = UJGProfileSaveGame::Load();
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UJGSettingsSubsystem::HandlePostLoadMap);
}

void UJGSettingsSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);

	Super::Deinitialize();
}

UJGSettingsSubsystem* UJGSettingsSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UJGSettingsSubsystem>() : nullptr;
}

void UJGSettingsSubsystem::HandlePostLoadMap(UWorld* World)
{
	// 믹스 덮어쓰기는 오디오 장치에 걸리지만, 맵을 바꾸면 장치가 바뀔 수 있어서 다시 건다
	if (World && World->GetGameInstance() == GetGameInstance())
	{
		ApplyVolumes(World);
	}
}

#pragma endregion

#pragma region 볼륨

float& UJGSettingsSubsystem::VolumeRef(EJGVolumeChannel Channel) const
{
	switch (Channel)
	{
	case EJGVolumeChannel::Effects: return Profile->EffectsVolume;
	case EJGVolumeChannel::Music: return Profile->MusicVolume;
	default: return Profile->MasterVolume;
	}
}

void UJGSettingsSubsystem::SetVolume(EJGVolumeChannel Channel, float Volume, bool bSave)
{
	VolumeRef(Channel) = FMath::Clamp(Volume, 0.0f, 1.0f);
	ApplyVolumes(GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr);

	if (bSave)
	{
		SaveProfile();
	}
}

float UJGSettingsSubsystem::GetVolume(EJGVolumeChannel Channel) const
{
	return VolumeRef(Channel);
}

void UJGSettingsSubsystem::ApplyVolumes(UWorld* World) const
{
	const UJGGameSettings* Settings = GetDefault<UJGGameSettings>();
	USoundMix* Mix = Settings ? Settings->VolumeMix.LoadSynchronous() : nullptr;
	if (!World || !Mix)
	{
		return;
	}

	const TPair<USoundClass*, float> Channels[] = {
		{ Settings->MasterSoundClass.LoadSynchronous(), Profile->MasterVolume },
		{ Settings->EffectsSoundClass.LoadSynchronous(), Profile->EffectsVolume },
		{ Settings->MusicSoundClass.LoadSynchronous(), Profile->MusicVolume },
	};

	for (const TPair<USoundClass*, float>& Channel : Channels)
	{
		if (Channel.Key)
		{
			// 자식 클래스에도 적용해야 전체 볼륨이 효과음·배경음을 함께 줄인다
			UGameplayStatics::SetSoundMixClassOverride(World, Mix, Channel.Key, Channel.Value, 1.0f, 0.0f, true);
		}
	}

	UGameplayStatics::PushSoundMixModifier(World, Mix);
}

#pragma endregion

#pragma region 그래픽

void UJGSettingsSubsystem::SetGraphicsQuality(EJGGraphicsQuality Quality)
{
	UGameUserSettings* UserSettings = UGameUserSettings::GetGameUserSettings();
	if (!UserSettings)
	{
		return;
	}

	UserSettings->SetOverallScalabilityLevel(static_cast<int32>(Quality));
	UserSettings->ApplySettings(false);
	UserSettings->SaveSettings();

	UE_LOG(LogDoJokgu, Log, TEXT("Graphics quality set to %s"), *UEnum::GetValueAsString(Quality));
}

EJGGraphicsQuality UJGSettingsSubsystem::GetGraphicsQuality() const
{
	const UGameUserSettings* UserSettings = UGameUserSettings::GetGameUserSettings();
	const int32 Level = UserSettings ? UserSettings->GetOverallScalabilityLevel() : -1;

	if (Level < 0 || Level >= static_cast<int32>(EJGGraphicsQuality::High))
	{
		return EJGGraphicsQuality::High;
	}

	return static_cast<EJGGraphicsQuality>(Level);
}

#pragma endregion

#pragma region 선수와 저장

FName UJGSettingsSubsystem::GetLastCharacterId() const
{
	return Profile->LastCharacterId;
}

void UJGSettingsSubsystem::SetLastCharacterId(FName CharacterId)
{
	Profile->LastCharacterId = CharacterId;
	SaveProfile();
}

void UJGSettingsSubsystem::SaveProfile()
{
	if (!Profile->Save())
	{
		UE_LOG(LogDoJokgu, Warning, TEXT("Could not write the profile save (%s)"), UJGProfileSaveGame::SlotName);
	}
}

#pragma endregion
