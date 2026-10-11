#include "Jokgu/UI/JGSlideButtonWidget.h"
#include "Jokgu/Player/JGCharacter.h"
#include "Rendering/DrawElements.h"

UJGSlideButtonWidget::UJGSlideButtonWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 히트 테스트를 받아야 터치가 들어온다
	SetVisibility(ESlateVisibility::Visible);
}

#pragma region 포인터 이벤트

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

#pragma endregion

#pragma region 누름 처리와 그리기

FReply UJGSlideButtonWidget::Press(const FPointerEvent& InEvent)
{
	// 버튼 위의 두 번째 손가락은 삼켜서 아래 공격 영역의 제스처가 시작되지 않게 한다
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
	// CanMoveNow가 슬라이딩 상태, 경기 종료, 서브하는 쪽을 함께 확인한다. 단계 검사는 서버가 따로 한다.
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

#pragma endregion
