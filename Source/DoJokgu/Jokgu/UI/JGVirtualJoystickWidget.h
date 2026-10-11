#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "JGVirtualJoystickWidget.generated.h"

class AJGCharacter;

/**
 *  왼손 이동 스틱. 공격 영역과 따로 자기 포인터를 추적해서 두 손을 동시에 쓸 수 있고,
 *  매 프레임 스틱 값을 AJGCharacter::DoMove로 넘긴다.
 */
UCLASS()
class UJGVirtualJoystickWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	UJGVirtualJoystickWidget(const FObjectInitializer& ObjectInitializer);

	/** 스틱을 놓고 값을 0으로 되돌린다 */
	UFUNCTION(BlueprintCallable, Category="Joystick")
	void ReleaseStick();

protected:

#pragma region 모양과 설정

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
	FSlateBrush BaseBrush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
	FSlateBrush ThumbBrush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
	FLinearColor BaseColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.15f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
	FLinearColor ThumbColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.6f);

	/** 손잡이가 움직일 수 있는 거리. 위젯 반 크기에 대한 비율이다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Joystick", meta=(ClampMin=0.1, ClampMax=1))
	float TravelRatio = 0.8f;

	/** 이 길이보다 작은 스틱 값은 무시한다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Joystick", meta=(ClampMin=0, ClampMax=0.9))
	float DeadZone = 0.12f;

#pragma endregion

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

#pragma region 포인터 이벤트

	virtual FReply NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

#pragma endregion

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	/** 포인터 위치를 위젯 중심 기준 [-1, 1] 원 안의 값으로 바꾼다 */
	void UpdateStick(const FGeometry& InGeometry, const FPointerEvent& InEvent);
	void HandleApplicationWillDeactivate();

	bool bPointerActive = false;
	int32 ActivePointerIndex = INDEX_NONE;

	/** 정규화된 스틱 값. X는 오른쪽, Y는 아래쪽(화면 좌표) */
	FVector2D StickValue = FVector2D::ZeroVector;
};
