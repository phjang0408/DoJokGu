#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "JGLookDragWidget.generated.h"

/**
 *  로비 카메라 회전 영역. 손가락(또는 마우스 오른쪽 버튼)으로 좌우로 끌면 로비 캐릭터의 카메라가 돈다.
 *  조이스틱과 따로 자기 포인터를 추적해서 두 손을 동시에 쓸 수 있다.
 */
UCLASS()
class UJGLookDragWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	UJGLookDragWidget(const FObjectInitializer& ObjectInitializer);

protected:

	/** 화면 1픽셀당 회전 각도 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Look", meta=(ClampMin=0.01, ClampMax=2))
	float DegreesPerPixel = 0.25f;

	virtual FReply NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

	FReply BeginDrag(const FPointerEvent& InEvent);
	FReply ContinueDrag(const FPointerEvent& InEvent);
	FReply EndDrag(const FPointerEvent& InEvent);

	bool bDragging = false;
	int32 ActivePointerIndex = INDEX_NONE;
	FVector2D LastScreenPosition = FVector2D::ZeroVector;
};
