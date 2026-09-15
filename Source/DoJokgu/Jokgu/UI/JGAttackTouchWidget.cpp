#include "Jokgu/UI/JGAttackTouchWidget.h"
#include "Jokgu/Gameplay/JGShotCalculator.h"
#include "Jokgu/Data/JGBalanceData.h"
#include "Jokgu/Player/JGCharacter.h"
#include "Misc/CoreDelegates.h"
#include "Rendering/DrawElements.h"
#include "HAL/PlatformTime.h"

UJGAttackTouchWidget::UJGAttackTouchWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetVisibility(ESlateVisibility::Visible);
}

void UJGAttackTouchWidget::NativeConstruct()
{
	Super::NativeConstruct();

	FCoreDelegates::ApplicationWillDeactivateDelegate.AddUObject(this, &UJGAttackTouchWidget::HandleApplicationWillDeactivate);
}

void UJGAttackTouchWidget::NativeDestruct()
{
	FCoreDelegates::ApplicationWillDeactivateDelegate.RemoveAll(this);
	CancelActiveTouch();

	Super::NativeDestruct();
}

FReply UJGAttackTouchWidget::NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	return BeginPointer(InGeometry, InGestureEvent);
}

FReply UJGAttackTouchWidget::NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	return MovePointer(InGeometry, InGestureEvent);
}

FReply UJGAttackTouchWidget::NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	return EndPointer(InGeometry, InGestureEvent);
}

FReply UJGAttackTouchWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.IsTouchEvent() || InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	return BeginPointer(InGeometry, InMouseEvent);
}

FReply UJGAttackTouchWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.IsTouchEvent())
	{
		return FReply::Unhandled();
	}

	return MovePointer(InGeometry, InMouseEvent);
}

FReply UJGAttackTouchWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.IsTouchEvent() || InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	return EndPointer(InGeometry, InMouseEvent);
}

void UJGAttackTouchWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);

	CancelActiveTouch();
}

FReply UJGAttackTouchWidget::BeginPointer(const FGeometry& InGeometry, const FPointerEvent& InEvent)
{
	// a second finger inside the zone is swallowed, the first one keeps control
	if (bPointerActive)
	{
		return FReply::Handled();
	}

	bPointerActive = true;
	bActivePointerIsTouch = InEvent.IsTouchEvent();
	ActivePointerIndex = InEvent.GetPointerIndex();
	StartLocal = InGeometry.AbsoluteToLocal(InEvent.GetScreenSpacePosition());
	CurrentLocal = StartLocal;
	StartTime = FPlatformTime::Seconds();

	return FReply::Handled().CaptureMouse(TakeWidget());
}

FReply UJGAttackTouchWidget::MovePointer(const FGeometry& InGeometry, const FPointerEvent& InEvent)
{
	if (!IsActivePointer(InEvent))
	{
		return FReply::Unhandled();
	}

	CurrentLocal = InGeometry.AbsoluteToLocal(InEvent.GetScreenSpacePosition());

	if (AJGCharacter* Character = GetControlledCharacter())
	{
		const FVector2D Delta = CurrentLocal - StartLocal;
		if (Delta.Size() > UJGBalanceData::Get()->DragDeadZone)
		{
			Character->SetAimPreview(true, UJGShotCalculator::ComputeAimAngle(Delta), UJGShotCalculator::ComputeDragPower(Delta.Size()));
		}
		else
		{
			Character->SetAimPreview(false, 0.0f, 0.0f);
		}
	}

	return FReply::Handled();
}

FReply UJGAttackTouchWidget::EndPointer(const FGeometry& InGeometry, const FPointerEvent& InEvent)
{
	if (!IsActivePointer(InEvent))
	{
		return FReply::Unhandled();
	}

	CurrentLocal = InGeometry.AbsoluteToLocal(InEvent.GetScreenSpacePosition());

	const FVector2D Delta = CurrentLocal - StartLocal;
	const float HeldSeconds = static_cast<float>(FPlatformTime::Seconds() - StartTime);

	bPointerActive = false;
	ActivePointerIndex = INDEX_NONE;

	if (AJGCharacter* Character = GetControlledCharacter())
	{
		Character->SetAimPreview(false, 0.0f, 0.0f);

		FJGAttackInput Input;
		if (UJGShotCalculator::ResolveTouchGesture(Delta, HeldSeconds, Input))
		{
			Character->SubmitAttack(Input);
		}
	}

	return FReply::Handled().ReleaseMouseCapture();
}

bool UJGAttackTouchWidget::IsActivePointer(const FPointerEvent& InEvent) const
{
	return bPointerActive
		&& InEvent.IsTouchEvent() == bActivePointerIsTouch
		&& InEvent.GetPointerIndex() == ActivePointerIndex;
}

void UJGAttackTouchWidget::CancelActiveTouch()
{
	if (!bPointerActive)
	{
		return;
	}

	bPointerActive = false;
	ActivePointerIndex = INDEX_NONE;

	if (AJGCharacter* Character = GetControlledCharacter())
	{
		Character->SetAimPreview(false, 0.0f, 0.0f);
	}
}

void UJGAttackTouchWidget::HandleApplicationWillDeactivate()
{
	CancelActiveTouch();
}

AJGCharacter* UJGAttackTouchWidget::GetControlledCharacter() const
{
	return GetOwningPlayerPawn<AJGCharacter>();
}

int32 UJGAttackTouchWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 MaxLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	++MaxLayer;
	FSlateDrawElement::MakeBox(OutDrawElements, MaxLayer, AllottedGeometry.ToPaintGeometry(), &ZoneBrush, ESlateDrawEffect::None,
		ZoneColor * InWidgetStyle.GetColorAndOpacityTint());

	if (bPointerActive)
	{
		++MaxLayer;

		TArray<FVector2f> Points;
		Points.Add(FVector2f(StartLocal));
		Points.Add(FVector2f(CurrentLocal));
		FSlateDrawElement::MakeLines(OutDrawElements, MaxLayer, AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, DragColor, true, 4.0f);

		const FVector2f MarkerSize(24.0f, 24.0f);
		FSlateDrawElement::MakeBox(OutDrawElements, MaxLayer,
			AllottedGeometry.ToPaintGeometry(MarkerSize, FSlateLayoutTransform(FVector2f(StartLocal) - MarkerSize * 0.5f)),
			&ZoneBrush, ESlateDrawEffect::None, DragColor);
	}

	return MaxLayer;
}
