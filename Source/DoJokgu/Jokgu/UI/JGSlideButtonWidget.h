#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "JGSlideButtonWidget.generated.h"

/**
 *  오른손 슬라이딩 버튼. 몸을 날리는 동작이 늦지 않도록 뗄 때가 아니라 누르는 순간 AJGCharacter::DoSlide를 호출하고,
 *  포인터를 소비해서 아래의 공격 영역이 그 터치를 받지 않게 한다. 슬라이딩이 Ready가 아니면 흐리게 그린다.
 */
UCLASS()
class UJGSlideButtonWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	UJGSlideButtonWidget(const FObjectInitializer& ObjectInitializer);

protected:

#pragma region 모양

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
	FSlateBrush ButtonBrush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
	FLinearColor ReadyColor = FLinearColor(0.3f, 0.8f, 1.0f, 0.55f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
	FLinearColor PressedColor = FLinearColor(0.3f, 0.8f, 1.0f, 0.85f);

	/** 지금 슬라이딩할 수 없을 때의 색 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
	FLinearColor UnavailableColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.12f);

#pragma endregion

#pragma region 포인터 이벤트

	virtual FReply NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

#pragma endregion

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	FReply Press(const FPointerEvent& InEvent);
	FReply Release(const FPointerEvent& InEvent);

	/** 버튼 색을 정하는 로컬 판단. 실제 허용 여부는 서버가 다시 검사한다. */
	bool IsSlideReady() const;

	bool bPressed = false;
	int32 ActivePointerIndex = INDEX_NONE;
};
