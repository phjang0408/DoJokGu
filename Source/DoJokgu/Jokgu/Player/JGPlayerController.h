#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "JGPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UJGMatchHUDWidget;

/**
 *  Local input setup, HUD creation and the entry point for match requests that are not tied to the pawn.
 *  UI is only created on the local controller.
 */
UCLASS()
class AJGPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:

	UPROPERTY(EditAnywhere, Category="Input")
	TArray<TObjectPtr<UInputMappingContext>> DefaultMappingContexts;

	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> RematchAction;

	/** HUD class. Falls back to the native UJGMatchHUDWidget layout if unset. */
	UPROPERTY(EditAnywhere, Category="UI")
	TSubclassOf<UJGMatchHUDWidget> HUDWidgetClass;

	/** Shows the virtual joystick on desktop as well */
	UPROPERTY(EditAnywhere, Config, Category="UI")
	bool bForceTouchControls = false;

	UPROPERTY(Transient)
	TObjectPtr<UJGMatchHUDWidget> HUDWidget;

public:

	UFUNCTION(BlueprintCallable, Category="Match")
	void RequestRematch();

	UFUNCTION(BlueprintPure, Category="UI")
	bool ShouldUseTouchControls() const;

	UFUNCTION(BlueprintPure, Category="UI")
	UJGMatchHUDWidget* GetHUDWidget() const { return HUDWidget; }

protected:

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	UFUNCTION(Server, Reliable)
	void ServerRequestRematch();
};
