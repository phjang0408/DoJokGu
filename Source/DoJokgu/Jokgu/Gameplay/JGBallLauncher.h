#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Jokgu/Core/JGTypes.h"
#include "JGBallLauncher.generated.h"

class UArrowComponent;
class UStaticMeshComponent;

USTRUCT(BlueprintType)
struct FJGLaunchPreset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Launch")
	FName Label = TEXT("Medium");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Launch")
	FJGAttackInput Input;
};

/**
 *  Solo test device: repeatedly fires the match ball with the same shot model as a player hit.
 *  Only works when the game mode runs in practice mode. Not a base for an AI opponent.
 *  Place it on the hitter's side; ReceiverTeam is the side that should receive the ball.
 */
UCLASS()
class AJGBallLauncher : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> BodyMesh;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UArrowComponent> Arrow;
#endif

protected:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Launcher")
	EJGTeam ReceiverTeam = EJGTeam::A;

	/** Launch height of the ball above the launcher origin */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Launcher", meta=(Units="cm"))
	float LaunchHeight = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Launcher")
	bool bAutoLaunch = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Launcher", meta=(ClampMin=0.2, Units="s"))
	float LaunchInterval = 3.0f;

	/** Presets are fired in order and loop. Defaults: weak / medium / strong drag and a tap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Launcher")
	TArray<FJGLaunchPreset> Presets;

public:

	AJGBallLauncher();

	/** Fires the next preset (server only) */
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Launcher")
	void LaunchNext();

protected:

	virtual void BeginPlay() override;

	FTimerHandle LaunchTimer;
	int32 NextPresetIndex = 0;
};
