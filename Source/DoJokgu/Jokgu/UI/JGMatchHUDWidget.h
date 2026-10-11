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
class UJGSlideButtonWidget;

/**
 *  경기 HUD: 점수, 단계·카운트다운, 득점 사유, 결과와 재경기, 조이스틱, 슬라이딩 버튼, 공격 영역.
 *  위젯 트리가 비어 있으면(C++ 클래스 그대로이거나 빈 WBP) 기본 임시 레이아웃을 코드로 만든다.
 *  자식 WBP는 아래 BindWidgetOptional 프로퍼티와 같은 이름의 위젯을 두어 자체 레이아웃을 쓸 수 있다.
 */
UCLASS()
class UJGMatchHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** 조이스틱, 슬라이딩 버튼, SLIDE 글자를 보이거나 숨긴다. 공격 영역은 마우스 테스트용으로 항상 보인다. */
	void SetTouchControlsVisible(bool bVisible);

protected:

#pragma region 바인딩 위젯

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

	/** 터치 전용이며 조이스틱과 함께 보인다. 터치를 먼저 받도록 위젯 순서에서 AttackZone보다 위에 있어야 한다. */
	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UJGSlideButtonWidget> SlideButton;

#pragma endregion

	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

#pragma region 경기 상태 표시

	/** Host와 클라이언트가 함께 쓰는 단일 갱신 경로 */
	UFUNCTION(BlueprintNativeEvent, Category="HUD")
	void RefreshMatchState(AJGGameStateBase* GameState);

	UFUNCTION()
	void HandleMatchStateChanged(AJGGameStateBase* GameState);

	UFUNCTION()
	void HandleRematchClicked();

	void BuildDefaultLayout();

	/** 게임 스테이트가 생기면 한 번 구독하고 바로 갱신한다 */
	void TryBindGameState();

	/** 카운트다운이 들어가므로 매 틱 갱신한다 */
	void UpdatePhaseText(const AJGGameStateBase& GameState);

	EJGTeam GetLocalTeam() const;

	static FString TeamName(EJGTeam Team);
	static FString ReasonName(EJGPointReason Reason);

#pragma endregion

	TWeakObjectPtr<AJGGameStateBase> BoundGameState;

	/** 레이아웃이 만들어지기 전에 설정된 값을 NativeOnInitialized에서 적용하려고 기억해 둔다 */
	bool bTouchControlsVisible = false;
};
