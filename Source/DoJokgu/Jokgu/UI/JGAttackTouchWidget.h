#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "JGAttackTouchWidget.generated.h"

class AJGCharacter;

/**
 *  Right hand attack area. Tracks one pointer (touch PointerIndex, or the left mouse button on PC):
 *  press stores start position/time, move updates the aim arrow, release sends one tap or drag attack.
 *  Distances are measured in the widget's local (DPI scaled) space. The pointer is captured so dragging
 *  outside the area keeps working. Capture loss and app deactivation cancel without attacking.
 */
UCLASS()
class UJGAttackTouchWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	UJGAttackTouchWidget(const FObjectInitializer& ObjectInitializer);

	/** Clears the active gesture and the aim arrow without attacking */
	UFUNCTION(BlueprintCallable, Category="Attack")
	void CancelActiveTouch();

protected:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
	FSlateBrush ZoneBrush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
	FLinearColor ZoneColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.03f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
	FLinearColor DragColor = FLinearColor(1.0f, 0.8f, 0.2f, 0.8f);

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	virtual FReply NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;

	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	FReply BeginPointer(const FGeometry& InGeometry, const FPointerEvent& InEvent);
	FReply MovePointer(const FGeometry& InGeometry, const FPointerEvent& InEvent);
	FReply EndPointer(const FGeometry& InGeometry, const FPointerEvent& InEvent);
	bool IsActivePointer(const FPointerEvent& InEvent) const;

	void HandleApplicationWillDeactivate();
	AJGCharacter* GetControlledCharacter() const;

	bool bPointerActive = false;
	bool bActivePointerIsTouch = false;
	int32 ActivePointerIndex = INDEX_NONE;
	FVector2D StartLocal = FVector2D::ZeroVector;
	FVector2D CurrentLocal = FVector2D::ZeroVector;
	double StartTime = 0.0;
};
