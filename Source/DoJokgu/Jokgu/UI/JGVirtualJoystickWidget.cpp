#include "Jokgu/UI/JGVirtualJoystickWidget.h"
#include "Jokgu/Player/JGCharacter.h"
#include "Misc/CoreDelegates.h"
#include "Rendering/DrawElements.h"

UJGVirtualJoystickWidget::UJGVirtualJoystickWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetVisibility(ESlateVisibility::Visible);
}

void UJGVirtualJoystickWidget::NativeConstruct()
{
	Super::NativeConstruct();

	FCoreDelegates::ApplicationWillDeactivateDelegate.AddUObject(this, &UJGVirtualJoystickWidget::HandleApplicationWillDeactivate);
}

void UJGVirtualJoystickWidget::NativeDestruct()
{
	FCoreDelegates::ApplicationWillDeactivateDelegate.RemoveAll(this);
	ReleaseStick();

	Super::NativeDestruct();
}

void UJGVirtualJoystickWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bPointerActive || StickValue.Size() < DeadZone)
	{
		return;
	}

	if (AJGCharacter* Character = GetOwningPlayerPawn<AJGCharacter>())
	{
		// screen up is forward
		Character->DoMove(StickValue.X, -StickValue.Y);
	}
}

FReply UJGVirtualJoystickWidget::NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	if (bPointerActive)
	{
		return FReply::Handled();
	}

	bPointerActive = true;
	ActivePointerIndex = InGestureEvent.GetPointerIndex();
	UpdateStick(InGeometry, InGestureEvent);

	return FReply::Handled().CaptureMouse(TakeWidget());
}

FReply UJGVirtualJoystickWidget::NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	if (!bPointerActive || InGestureEvent.GetPointerIndex() != ActivePointerIndex)
	{
		return FReply::Unhandled();
	}

	UpdateStick(InGeometry, InGestureEvent);
	return FReply::Handled();
}

FReply UJGVirtualJoystickWidget::NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	if (!bPointerActive || InGestureEvent.GetPointerIndex() != ActivePointerIndex)
	{
		return FReply::Unhandled();
	}

	ReleaseStick();
	return FReply::Handled().ReleaseMouseCapture();
}

void UJGVirtualJoystickWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);

	ReleaseStick();
}

void UJGVirtualJoystickWidget::UpdateStick(const FGeometry& InGeometry, const FPointerEvent& InEvent)
{
	const FVector2D Size = InGeometry.GetLocalSize();
	const FVector2D Center = Size * 0.5;
	const double Radius = FMath::Max(1.0, FMath::Min(Size.X, Size.Y) * 0.5 * TravelRatio);
	const FVector2D Local = InGeometry.AbsoluteToLocal(InEvent.GetScreenSpacePosition());

	StickValue = ((Local - Center) / Radius).ClampAxes(-1.0, 1.0);
	if (StickValue.SizeSquared() > 1.0)
	{
		StickValue.Normalize();
	}
}

void UJGVirtualJoystickWidget::ReleaseStick()
{
	bPointerActive = false;
	ActivePointerIndex = INDEX_NONE;
	StickValue = FVector2D::ZeroVector;
}

void UJGVirtualJoystickWidget::HandleApplicationWillDeactivate()
{
	ReleaseStick();
}

int32 UJGVirtualJoystickWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 MaxLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	const FVector2f Size = FVector2f(AllottedGeometry.GetLocalSize());
	const float Radius = FMath::Min(Size.X, Size.Y) * 0.5f * TravelRatio;
	const FVector2f ThumbSize = Size * 0.35f;
	const FVector2f ThumbCenter = Size * 0.5f + FVector2f(StickValue) * Radius;

	++MaxLayer;
	FSlateDrawElement::MakeBox(OutDrawElements, MaxLayer, AllottedGeometry.ToPaintGeometry(), &BaseBrush, ESlateDrawEffect::None, BaseColor);

	++MaxLayer;
	FSlateDrawElement::MakeBox(OutDrawElements, MaxLayer,
		AllottedGeometry.ToPaintGeometry(ThumbSize, FSlateLayoutTransform(ThumbCenter - ThumbSize * 0.5f)),
		&ThumbBrush, ESlateDrawEffect::None, ThumbColor);

	return MaxLayer;
}
