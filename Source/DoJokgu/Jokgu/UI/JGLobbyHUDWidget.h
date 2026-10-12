#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Jokgu/Lobby/JGInteractZone.h"
#include "JGLobbyHUDWidget.generated.h"

class UButton;
class UTextBlock;
class UJGLookDragWidget;
class UJGMatchPanelWidget;
class UJGSettingsPanelWidget;
class UJGVirtualJoystickWidget;

/**
 *  로비 HUD: 조이스틱, 카메라 드래그 영역, 구역 버튼(구역에 들어가면 보임), 오른쪽 위 바로가기 2개,
 *  경기 패널, 설정 패널, 그리고 경기에서 돌아온 이유(접속 끊김 등) 한 줄.
 *  위젯 트리가 비어 있으면 기본 레이아웃을 코드로 만든다.
 */
UCLASS()
class UJGLobbyHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	void SetTouchControlsVisible(bool bVisible);

	/** 지금 서 있는 구역. nullptr이면 구역 버튼을 숨긴다. */
	void SetActiveZone(const AJGInteractZone* Zone);

	/** 패널 하나를 연다(다른 패널은 닫는다) */
	UFUNCTION(BlueprintCallable, Category="Lobby")
	void OpenPanel(EJGZoneKind Kind);

	UFUNCTION(BlueprintCallable, Category="Lobby")
	void ClosePanels();

protected:

#pragma region 바인딩 위젯

	UPROPERTY(BlueprintReadOnly, Category="Lobby", meta=(BindWidgetOptional))
	TObjectPtr<UJGLookDragWidget> LookArea;

	UPROPERTY(BlueprintReadOnly, Category="Lobby", meta=(BindWidgetOptional))
	TObjectPtr<UJGVirtualJoystickWidget> Joystick;

	UPROPERTY(BlueprintReadOnly, Category="Lobby", meta=(BindWidgetOptional))
	TObjectPtr<UButton> ZoneButton;

	UPROPERTY(BlueprintReadOnly, Category="Lobby", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ZoneLabel;

	UPROPERTY(BlueprintReadOnly, Category="Lobby", meta=(BindWidgetOptional))
	TObjectPtr<UButton> PlayShortcut;

	UPROPERTY(BlueprintReadOnly, Category="Lobby", meta=(BindWidgetOptional))
	TObjectPtr<UButton> SettingsShortcut;

	UPROPERTY(BlueprintReadOnly, Category="Lobby", meta=(BindWidgetOptional))
	TObjectPtr<UJGMatchPanelWidget> MatchPanel;

	UPROPERTY(BlueprintReadOnly, Category="Lobby", meta=(BindWidgetOptional))
	TObjectPtr<UJGSettingsPanelWidget> SettingsPanel;

	UPROPERTY(BlueprintReadOnly, Category="Lobby", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> MessageText;

#pragma endregion

	virtual void NativeOnInitialized() override;

	void BuildDefaultLayout();

	/** 경기에서 실패로 돌아왔으면 그 이유를 잠시 보여 준다 */
	void ShowReturnReason();

	UFUNCTION()
	void HandleZoneClicked();

	UFUNCTION()
	void HandlePlayShortcut();

	UFUNCTION()
	void HandleSettingsShortcut();

	UFUNCTION()
	void HideMessage();

	EJGZoneKind ActiveZoneKind = EJGZoneKind::Match;
	bool bTouchControlsVisible = false;

	FTimerHandle MessageTimer;
};
