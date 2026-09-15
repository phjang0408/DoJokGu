#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "JGVirtualJoystickWidget.generated.h"

class AJGCharacter;

/**
 *  Left hand movement stick. Tracks its own pointer independently from the attack zone so both hands work at once,
 *  and forwards the stick value to AJGCharacter::DoMove every frame.
 */
UCLASS()
class UJGVirtualJoystickWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	UJGVirtualJoystickWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category="Joystick")
	void ReleaseStick();

protected:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
	FSlateBrush BaseBrush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
	FSlateBrush ThumbBrush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
	FLinearColor BaseColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.15f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
	FLinearColor ThumbColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.6f);

	/** Fraction of the widget half size the thumb can travel */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Joystick", meta=(ClampMin=0.1, ClampMax=1))
	float TravelRatio = 0.8f;

	/** Stick values below this length are ignored */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Joystick", meta=(ClampMin=0, ClampMax=0.9))
	float DeadZone = 0.12f;

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	virtual FReply NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	void UpdateStick(const FGeometry& InGeometry, const FPointerEvent& InEvent);
	void HandleApplicationWillDeactivate();

	bool bPointerActive = false;
	int32 ActivePointerIndex = INDEX_NONE;

	/** Normalized stick, X right and Y down (screen space) */
	FVector2D StickValue = FVector2D::ZeroVector;
};
