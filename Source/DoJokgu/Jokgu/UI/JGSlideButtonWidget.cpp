#include "Jokgu/UI/JGSlideButtonWidget.h"
#include "Jokgu/Player/JGCharacter.h"
#include "Rendering/DrawElements.h"

UJGSlideButtonWidget::UJGSlideButtonWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetVisibility(ESlateVisibility::Visible);
}

FReply UJGSlideButtonWidget::NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	return Press(InGestureEvent);
}

FReply UJGSlideButtonWidget::NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	return Release(InGestureEvent);
}

FReply UJGSlideButtonWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.IsTouchEvent() || InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	return Press(InMouseEvent);
}

FReply UJGSlideButtonWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.IsTouchEvent() || InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	return Release(InMouseEvent);
}

void UJGSlideButtonWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);

	bPressed = false;
	ActivePointerIndex = INDEX_NONE;
}

FReply UJGSlideButtonWidget::Press(const FPointerEvent& InEvent)
{
	// a second finger on the button is swallowed so it cannot start an attack gesture underneath
	if (bPressed)
	{
		return FReply::Handled();
	}

	bPressed = true;
	ActivePointerIndex = InEvent.GetPointerIndex();

	if (AJGCharacter* Character = GetOwningPlayerPawn<AJGCharacter>())
	{
		Character->DoSlide();
	}

	return FReply::Handled().CaptureMouse(TakeWidget());
}

FReply UJGSlideButtonWidget::Release(const FPointerEvent& InEvent)
{
	if (!bPressed || InEvent.GetPointerIndex() != ActivePointerIndex)
	{
		return FReply::Unhandled();
	}

	bPressed = false;
	ActivePointerIndex = INDEX_NONE;
	return FReply::Handled().ReleaseMouseCapture();
}

bool UJGSlideButtonWidget::IsSlideReady() const
{
	const AJGCharacter* Character = GetOwningPlayerPawn<AJGCharacter>();
	// CanMoveNow covers the slide state, match end and the serving player; the server still checks the phase
	return Character && Character->CanMoveNow();
}

int32 UJGSlideButtonWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 MaxLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	const FLinearColor Color = !IsSlideReady() ? UnavailableColor : (bPressed ? PressedColor : ReadyColor);

	++MaxLayer;
	FSlateDrawElement::MakeBox(OutDrawElements, MaxLayer, AllottedGeometry.ToPaintGeometry(), &ButtonBrush, ESlateDrawEffect::None, Color);

	return MaxLayer;
}
