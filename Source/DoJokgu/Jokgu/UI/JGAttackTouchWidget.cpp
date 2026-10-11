#include "Jokgu/UI/JGAttackTouchWidget.h"
#include "Jokgu/Gameplay/JGShotCalculator.h"
#include "Jokgu/Data/JGBalanceData.h"
#include "Jokgu/Player/JGCharacter.h"
#include "Misc/CoreDelegates.h"
#include "Rendering/DrawElements.h"
#include "HAL/PlatformTime.h"

#pragma region 초기화

UJGAttackTouchWidget::UJGAttackTouchWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 히트 테스트를 받아야 터치가 들어온다
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

#pragma endregion

#pragma region 포인터 이벤트

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

// 마우스 함수는 터치가 마우스 이벤트로도 들어오는 경우를 걸러서 같은 터치를 두 번 처리하지 않는다

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

#pragma endregion

#pragma region 제스처 처리

FReply UJGAttackTouchWidget::BeginPointer(const FGeometry& InGeometry, const FPointerEvent& InEvent)
{
	// 영역 안의 두 번째 손가락은 삼키고, 첫 손가락이 계속 조작한다
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

	// 데드존을 넘었을 때만 서버로 보낼 것과 같은 각도·파워로 조준 화살표를 보여 준다
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

		// 움직이지 않고 길게 누른 경우는 취소로 보고 아무것도 보내지 않는다
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

#pragma endregion

#pragma region 그리기

int32 UJGAttackTouchWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 MaxLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	++MaxLayer;
	FSlateDrawElement::MakeBox(OutDrawElements, MaxLayer, AllottedGeometry.ToPaintGeometry(), &ZoneBrush, ESlateDrawEffect::None,
		ZoneColor * InWidgetStyle.GetColorAndOpacityTint());

	// 드래그 중에는 시작점 표시와 현재 위치까지의 선을 그린다
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

#pragma endregion
