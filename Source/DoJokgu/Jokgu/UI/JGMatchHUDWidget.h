#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Jokgu/Core/JGTypes.h"
#include "JGMatchHUDWidget.generated.h"

class UTextBlock;
class UButton;
class AJGGameStateBase;
class UJGAttackTouchWidget;
class UJGVirtualJoystickWidget;

/**
 *  Match HUD: score, phase / countdown, point reason, result + rematch, joystick and attack zone.
 *  If the widget tree is empty (native class or empty WBP) a default placeholder layout is built in code.
 *  A WBP child can provide its own layout by naming widgets like the BindWidgetOptional properties below.
 */
UCLASS()
class UJGMatchHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	void SetTouchControlsVisible(bool bVisible);

protected:

	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ScoreText;

	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> PhaseText;

	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> MessageText;

	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UButton> RematchButton;

	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UJGAttackTouchWidget> AttackZone;

	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UJGVirtualJoystickWidget> Joystick;

	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

	/** Single refresh path for host and clients */
	UFUNCTION(BlueprintNativeEvent, Category="HUD")
	void RefreshMatchState(AJGGameStateBase* GameState);

	UFUNCTION()
	void HandleMatchStateChanged(AJGGameStateBase* GameState);

	UFUNCTION()
	void HandleRematchClicked();

	void BuildDefaultLayout();
	void TryBindGameState();
	void UpdatePhaseText(const AJGGameStateBase& GameState);
	EJGTeam GetLocalTeam() const;

	static FString TeamName(EJGTeam Team);
	static FString ReasonName(EJGPointReason Reason);

	TWeakObjectPtr<AJGGameStateBase> BoundGameState;
	bool bTouchControlsVisible = false;
};
