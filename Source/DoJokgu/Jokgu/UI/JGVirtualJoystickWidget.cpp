#include "Jokgu/UI/JGVirtualJoystickWidget.h"
#include "Jokgu/Player/JGCharacter.h"
#include "Misc/CoreDelegates.h"
#include "Rendering/DrawElements.h"

#pragma region 초기화와 틱

UJGVirtualJoystickWidget::UJGVirtualJoystickWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 히트 테스트를 받아야 터치가 들어온다
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
		// 화면 위쪽이 앞쪽이다
		Character->DoMove(StickValue.X, -StickValue.Y);
	}
}

#pragma endregion

#pragma region 포인터 이벤트

FReply UJGVirtualJoystickWidget::NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	// 이미 잡은 손가락이 있으면 두 번째 손가락은 무시한다
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

#pragma endregion

#pragma region 스틱 값과 그리기

void UJGVirtualJoystickWidget::UpdateStick(const FGeometry& InGeometry, const FPointerEvent& InEvent)
{
	const FVector2D Size = InGeometry.GetLocalSize();
	const FVector2D Center = Size * 0.5;
	const double Radius = FMath::Max(1.0, FMath::Min(Size.X, Size.Y) * 0.5 * TravelRatio);
	const FVector2D Local = InGeometry.AbsoluteToLocal(InEvent.GetScreenSpacePosition());

	// 사각형으로 자른 뒤 길이가 1을 넘으면 원 위로 줄인다
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

	// 바탕
	++MaxLayer;
	FSlateDrawElement::MakeBox(OutDrawElements, MaxLayer, AllottedGeometry.ToPaintGeometry(), &BaseBrush, ESlateDrawEffect::None, BaseColor);

	// 스틱 값 위치의 손잡이
	++MaxLayer;
	FSlateDrawElement::MakeBox(OutDrawElements, MaxLayer,
		AllottedGeometry.ToPaintGeometry(ThumbSize, FSlateLayoutTransform(ThumbCenter - ThumbSize * 0.5f)),
		&ThumbBrush, ESlateDrawEffect::None, ThumbColor);

	return MaxLayer;
}

#pragma endregion
