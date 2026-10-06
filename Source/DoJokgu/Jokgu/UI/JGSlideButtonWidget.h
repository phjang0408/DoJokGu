#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "JGSlideButtonWidget.generated.h"

/**
 *  Right hand slide button. Fires AJGCharacter::DoSlide on press (not release) so a dive is not delayed,
 *  and consumes the pointer so the attack zone underneath never sees it. Dimmed while the slide is not Ready.
 */
UCLASS()
class UJGSlideButtonWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	UJGSlideButtonWidget(const FObjectInitializer& ObjectInitializer);

protected:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
	FSlateBrush ButtonBrush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
	FLinearColor ReadyColor = FLinearColor(0.3f, 0.8f, 1.0f, 0.55f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
	FLinearColor PressedColor = FLinearColor(0.3f, 0.8f, 1.0f, 0.85f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
	FLinearColor UnavailableColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.12f);

	virtual FReply NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	FReply Press(const FPointerEvent& InEvent);
	FReply Release(const FPointerEvent& InEvent);
	bool IsSlideReady() const;

	bool bPressed = false;
	int32 ActivePointerIndex = INDEX_NONE;
};
