#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "JGAttackTouchWidget.generated.h"

class AJGCharacter;

/**
 *  오른손 공격 영역. 포인터 하나를 추적한다 (터치는 PointerIndex, PC는 마우스 왼쪽 버튼):
 *  누르면 시작 위치와 시각을 기억하고, 움직이면 조준 화살표를 갱신하고, 떼면 탭 또는 드래그 공격을 한 번 보낸다.
 *  거리는 위젯 로컬(DPI 배율 적용) 좌표로 잰다. 포인터를 캡처하므로 영역 밖으로 끌어도 계속 동작한다.
 *  캡처를 잃거나 앱이 비활성화되면 공격 없이 취소한다.
 */
UCLASS()
class UJGAttackTouchWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	UJGAttackTouchWidget(const FObjectInitializer& ObjectInitializer);

	/** 공격하지 않고 진행 중인 제스처와 조준 화살표를 지운다 */
	UFUNCTION(BlueprintCallable, Category="Attack")
	void CancelActiveTouch();

protected:

#pragma region 모양

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
	FSlateBrush ZoneBrush;

	/** 영역 전체에 깔리는 아주 옅은 색. 영역 위치만 알려 준다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
	FLinearColor ZoneColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.03f);

	/** 드래그 중 시작점 표시와 선의 색 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
	FLinearColor DragColor = FLinearColor(1.0f, 0.8f, 0.2f, 0.8f);

#pragma endregion

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

#pragma region 포인터 이벤트

	virtual FReply NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;

	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

#pragma endregion

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

#pragma region 제스처 처리

	// 터치와 마우스 이벤트가 함께 쓰는 처리 함수

	FReply BeginPointer(const FGeometry& InGeometry, const FPointerEvent& InEvent);
	FReply MovePointer(const FGeometry& InGeometry, const FPointerEvent& InEvent);
	FReply EndPointer(const FGeometry& InGeometry, const FPointerEvent& InEvent);

	/** 처음 누른 포인터와 같은 종류·같은 번호의 이벤트인지 */
	bool IsActivePointer(const FPointerEvent& InEvent) const;

	void HandleApplicationWillDeactivate();
	AJGCharacter* GetControlledCharacter() const;

#pragma endregion

#pragma region 제스처 상태

	bool bPointerActive = false;
	bool bActivePointerIsTouch = false;
	int32 ActivePointerIndex = INDEX_NONE;
	FVector2D StartLocal = FVector2D::ZeroVector;
	FVector2D CurrentLocal = FVector2D::ZeroVector;

	/** 누른 시각(플랫폼 시간). 탭과 길게 누름을 가른다. */
	double StartTime = 0.0;

#pragma endregion
};
