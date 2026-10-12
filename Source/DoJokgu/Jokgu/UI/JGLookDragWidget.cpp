#include "Jokgu/UI/JGLookDragWidget.h"
#include "Jokgu/Lobby/JGLobbyCharacter.h"

UJGLookDragWidget::UJGLookDragWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 히트 테스트를 받아야 터치가 들어온다
	SetVisibility(ESlateVisibility::Visible);
}

FReply UJGLookDragWidget::BeginDrag(const FPointerEvent& InEvent)
{
	if (bDragging)
	{
		return FReply::Unhandled();
	}

	bDragging = true;
	ActivePointerIndex = InEvent.GetPointerIndex();
	LastScreenPosition = InEvent.GetScreenSpacePosition();
	return FReply::Handled().CaptureMouse(TakeWidget());
}

FReply UJGLookDragWidget::ContinueDrag(const FPointerEvent& InEvent)
{
	if (!bDragging || InEvent.GetPointerIndex() != ActivePointerIndex)
	{
		return FReply::Unhandled();
	}

	const FVector2D Position = InEvent.GetScreenSpacePosition();
	const float DeltaX = static_cast<float>(Position.X - LastScreenPosition.X);
	LastScreenPosition = Position;

	if (AJGLobbyCharacter* Character = GetOwningPlayerPawn<AJGLobbyCharacter>())
	{
		Character->AddCameraYaw(DeltaX * DegreesPerPixel);
	}

	return FReply::Handled();
}

FReply UJGLookDragWidget::EndDrag(const FPointerEvent& InEvent)
{
	if (!bDragging || InEvent.GetPointerIndex() != ActivePointerIndex)
	{
		return FReply::Unhandled();
	}

	bDragging = false;
	ActivePointerIndex = INDEX_NONE;
	return FReply::Handled().ReleaseMouseCapture();
}

FReply UJGLookDragWidget::NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	return BeginDrag(InGestureEvent);
}

FReply UJGLookDragWidget::NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	return ContinueDrag(InGestureEvent);
}

FReply UJGLookDragWidget::NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	return EndDrag(InGestureEvent);
}

FReply UJGLookDragWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// PC는 오른쪽 버튼으로 돌린다. 왼쪽 버튼을 잡으면 마우스 캡처 때문에 다른 버튼 클릭이 막힌다.
	return InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton ? BeginDrag(InMouseEvent) : FReply::Unhandled();
}

FReply UJGLookDragWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return ContinueDrag(InMouseEvent);
}

FReply UJGLookDragWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::RightMouseButton || !bDragging)
	{
		return FReply::Unhandled();
	}

	// 마우스는 포인터 번호가 누를 때와 다를 수 있으니 번호와 상관없이 캡처를 놓는다
	bDragging = false;
	ActivePointerIndex = INDEX_NONE;
	return FReply::Handled().ReleaseMouseCapture();
}

void UJGLookDragWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);

	bDragging = false;
	ActivePointerIndex = INDEX_NONE;
}
