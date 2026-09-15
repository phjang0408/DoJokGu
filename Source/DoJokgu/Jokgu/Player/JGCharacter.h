#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Jokgu/Core/JGTypes.h"
#include "JGCharacter.generated.h"

class UCameraComponent;
class UInputAction;
class UJGCharacterData;
class AJGBall;
class AJGGameStateBase;
struct FInputActionValue;

/**
 *  Jokgu player: CharacterMovement based movement, attack requests, input buffer, slide and the fixed match camera.
 *  Keyboard (Enhanced Input) and touch widgets both call DoMove / SubmitAttack / DoSlide.
 *  Hits are decided on the server: range + height check against the ball, then a new ball velocity.
 */
UCLASS()
class AJGCharacter : public ACharacter
{
	GENERATED_BODY()

	/** High fixed camera behind the player's own half. Uses absolute transforms, driven by the court. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCameraComponent> MatchCamera;

protected:

	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> SlideAction;

	/** PC test input: tap attack */
	UPROPERTY(EditAnywhere, Category="Input|Debug")
	TObjectPtr<UInputAction> DebugTapAction;

	/** PC test input: straight drag attack with DebugDragPower */
	UPROPERTY(EditAnywhere, Category="Input|Debug")
	TObjectPtr<UInputAction> DebugDragAction;

	UPROPERTY(EditAnywhere, Category="Input|Debug", meta=(ClampMin=0, ClampMax=1))
	float DebugDragPower = 0.5f;

	/** Draws a debug arrow for the drag aim until a real arrow mesh or decal is hooked to OnAimPreviewChanged */
	UPROPERTY(EditAnywhere, Category="Debug")
	bool bDrawPlaceholderAimArrow = true;

	UPROPERTY(ReplicatedUsing=OnRep_CharacterData, EditAnywhere, BlueprintReadOnly, Category="Jokgu")
	TObjectPtr<UJGCharacterData> CharacterData;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Jokgu")
	EJGSlideState SlideState = EJGSlideState::Ready;

public:

	AJGCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Movement relative to the match camera. Right/Forward in [-1, 1]. */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoMove(float Right, float Forward);

	/** Starts a slide in the last valid move direction (or toward the opponent if the player never moved) */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoSlide();

	/** Sends a confirmed tap/drag to the server. Local player only. */
	UFUNCTION(BlueprintCallable, Category="Input")
	void SubmitAttack(const FJGAttackInput& Input);

	/** Local aim arrow while dragging. Uses the same angle and power that will be sent to the server. */
	UFUNCTION(BlueprintCallable, Category="Input")
	void SetAimPreview(bool bVisible, float AimAngleDeg, float Power);

	UFUNCTION(BlueprintPure, Category="Jokgu")
	EJGTeam GetTeam() const;

	/** Direction from the own court toward the opponent court (not the facing direction) */
	UFUNCTION(BlueprintPure, Category="Jokgu")
	FVector GetAttackForward() const;

	UFUNCTION(BlueprintPure, Category="Jokgu")
	EJGSlideState GetSlideState() const { return SlideState; }

	UFUNCTION(BlueprintPure, Category="Jokgu")
	UJGCharacterData* GetCharacterData() const { return CharacterData; }

	UFUNCTION(BlueprintPure, Category="Jokgu")
	FVector GetFootLocation() const;

	/** Server: assigns data and applies stats */
	void SetCharacterData(UJGCharacterData* NewData);

	/** Server: clears buffered input, slide state and hit cooldown for a new point */
	void ResetForNewPoint();

protected:

	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	void Move(const FInputActionValue& Value);
	void OnDebugTap();
	void OnDebugDrag();

	UFUNCTION(Server, Reliable)
	void ServerSubmitAttack(const FJGAttackInput& Input);

	UFUNCTION(Server, Reliable)
	void ServerStartSlide(FVector_NetQuantizeNormal Direction);

	UFUNCTION(Client, Reliable)
	void ClientResetForNewPoint();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayKickMotion(EJGKickMotion Motion);

	/** Visual hook for kicks (montage is played from the character data automatically) */
	UFUNCTION(BlueprintImplementableEvent, Category="Jokgu")
	void OnKickMotionPlayed(EJGKickMotion Motion);

	/** Visual hook for the aim arrow (mesh, decal or widget) */
	UFUNCTION(BlueprintImplementableEvent, Category="Jokgu")
	void OnAimPreviewChanged(bool bVisible, FVector WorldDirection, float Power);

	UFUNCTION()
	void OnRep_CharacterData();

	void ApplyCharacterData();

	void HandleAttackInputOnServer(const FJGAttackInput& Input);
	void TickServerHitCheck();
	bool TryExecuteBufferedAttack();
	void TryExecuteSlideSave();
	void ExecuteHit(AJGBall& Ball, const FJGAttackInput& Input, bool bIsServe);
	bool IsBallInReach(const AJGBall& Ball, float HorizontalReach, float MinHeight, float MaxHeight) const;
	int32 GetStat(int32 UJGCharacterData::* Stat) const;
	void ClearBufferedAttack();

	void StartSlide(const FVector& Direction);
	void EndSlideActive();
	void EndSlideRecovery();
	void CancelSlide();

	bool CanMoveNow() const;
	void UpdateMatchCamera(float DeltaSeconds);
	void DrawAimPreview() const;
	void DrawDebugReach() const;

	AJGGameStateBase* GetJGGameState() const;

	FJGAttackInput BufferedAttack;
	double BufferedAttackExpireTime = 0.0;
	double NextAttackAllowedTime = 0.0;
	int32 NextInputId = 0;

	FVector LastMoveDirection = FVector::ZeroVector;
	FTimerHandle SlideTimer;
	bool bSlideSaveUsed = false;

	bool bAimPreviewVisible = false;
	float AimPreviewAngle = 0.0f;
	float AimPreviewPower = 0.0f;

	bool bCameraInitialized = false;
	EJGTeam CameraTeam = EJGTeam::None;
};
