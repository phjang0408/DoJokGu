#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Jokgu/Core/JGSettingsSubsystem.h"
#include "JGSettingsPanelWidget.generated.h"

class UButton;
class USlider;
class UTextBlock;

DECLARE_MULTICAST_DELEGATE(FJGPanelClosedSignature);

/**
 *  설정 패널: 볼륨 슬라이더 세 개(전체·효과음·배경음), 그래픽 품질 세 단계, 닫기.
 *  값은 UJGSettingsSubsystem이 적용하고 저장한다. 슬라이더를 끄는 동안에는 소리에만 적용하고,
 *  손을 떼면 저장한다. 위젯 트리가 비어 있으면 기본 레이아웃을 코드로 만든다.
 */
UCLASS()
class UJGSettingsPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** 닫기 버튼을 누르면 알린다. 패널을 숨기는 것은 소유 HUD가 한다. */
	FJGPanelClosedSignature OnCloseRequested;

	/** 열 때 저장된 값으로 슬라이더와 품질 강조를 맞춘다 */
	void SyncFromSettings();

protected:

#pragma region 바인딩 위젯

	UPROPERTY(BlueprintReadOnly, Category="Settings", meta=(BindWidgetOptional))
	TObjectPtr<USlider> MasterSlider;

	UPROPERTY(BlueprintReadOnly, Category="Settings", meta=(BindWidgetOptional))
	TObjectPtr<USlider> EffectsSlider;

	UPROPERTY(BlueprintReadOnly, Category="Settings", meta=(BindWidgetOptional))
	TObjectPtr<USlider> MusicSlider;

	UPROPERTY(BlueprintReadOnly, Category="Settings", meta=(BindWidgetOptional))
	TObjectPtr<UButton> LowButton;

	UPROPERTY(BlueprintReadOnly, Category="Settings", meta=(BindWidgetOptional))
	TObjectPtr<UButton> MediumButton;

	UPROPERTY(BlueprintReadOnly, Category="Settings", meta=(BindWidgetOptional))
	TObjectPtr<UButton> HighButton;

	UPROPERTY(BlueprintReadOnly, Category="Settings", meta=(BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

#pragma endregion

	virtual void NativeOnInitialized() override;

	void BuildDefaultLayout();

	/** 고른 품질 버튼만 강조한다 */
	void HighlightQuality(EJGGraphicsQuality Quality);

	void ChangeVolume(EJGVolumeChannel Channel, float Value);
	void PickQuality(EJGGraphicsQuality Quality);

#pragma region 위젯 이벤트

	UFUNCTION()
	void HandleMasterChanged(float Value);

	UFUNCTION()
	void HandleEffectsChanged(float Value);

	UFUNCTION()
	void HandleMusicChanged(float Value);

	UFUNCTION()
	void HandleSliderReleased();

	UFUNCTION()
	void HandleLowClicked();

	UFUNCTION()
	void HandleMediumClicked();

	UFUNCTION()
	void HandleHighClicked();

	UFUNCTION()
	void HandleCloseClicked();

#pragma endregion
};
