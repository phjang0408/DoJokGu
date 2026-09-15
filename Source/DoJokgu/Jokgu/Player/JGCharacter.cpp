#include "Jokgu/Player/JGCharacter.h"
#include "Jokgu/Core/JGDebug.h"
#include "Jokgu/Core/JGGameModeBase.h"
#include "Jokgu/Core/JGGameStateBase.h"
#include "Jokgu/Core/JGPlayerState.h"
#include "Jokgu/Data/JGBalanceData.h"
#include "Jokgu/Data/JGCharacterData.h"
#include "Jokgu/Gameplay/JGBall.h"
#include "Jokgu/Gameplay/JGCourt.h"
#include "Jokgu/Gameplay/JGShotCalculator.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "Animation/AnimMontage.h"
#include "Net/UnrealNetwork.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "DoJokgu.h"

namespace
{
	const FName SlideRootMotionName(TEXT("JGSlide"));
	constexpr int32 DefaultStat = 3;
}

AJGCharacter::AJGCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
	Movement->MaxWalkSpeed = 450.0f;
	Movement->MinAnalogWalkSpeed = 20.0f;
	Movement->BrakingDecelerationWalking = 2500.0f;
	Movement->GetNavAgentPropertiesRef().bCanJump = false;

	MatchCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("MatchCamera"));
	MatchCamera->SetupAttachment(RootComponent);
	MatchCamera->SetUsingAbsoluteLocation(true);
	MatchCamera->SetUsingAbsoluteRotation(true);
	MatchCamera->bUsePawnControlRotation = false;

	// Note: skeletal mesh and anim blueprint are set in BP_JGCharacter or through UJGCharacterData
}

void AJGCharacter::BeginPlay()
{
	Super::BeginPlay();

	MatchCamera->SetFieldOfView(UJGBalanceData::Get()->CameraFieldOfView);
	ApplyCharacterData();
}

void AJGCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AJGCharacter, CharacterData);
	DOREPLIFETIME(AJGCharacter, SlideState);
}

void AJGCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInputComponent)
	{
		UE_LOG(LogDoJokgu, Error, TEXT("'%s' requires an Enhanced Input component."), *GetNameSafe(this));
		return;
	}

	if (MoveAction)
	{
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AJGCharacter::Move);
	}

	if (SlideAction)
	{
		EnhancedInputComponent->BindAction(SlideAction, ETriggerEvent::Started, this, &AJGCharacter::DoSlide);
	}

	if (DebugTapAction)
	{
		EnhancedInputComponent->BindAction(DebugTapAction, ETriggerEvent::Started, this, &AJGCharacter::OnDebugTap);
	}

	if (DebugDragAction)
	{
		EnhancedInputComponent->BindAction(DebugDragAction, ETriggerEvent::Started, this, &AJGCharacter::OnDebugDrag);
	}
}

void AJGCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D MovementVector = Value.Get<FVector2D>();
	DoMove(MovementVector.X, MovementVector.Y);
}

void AJGCharacter::OnDebugTap()
{
	FJGAttackInput Input;
	Input.Type = EJGAttackType::Tap;
	SubmitAttack(Input);
}

void AJGCharacter::OnDebugDrag()
{
	FJGAttackInput Input;
	Input.Type = EJGAttackType::Drag;
	Input.Power = DebugDragPower;
	SubmitAttack(Input);
}

void AJGCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority())
	{
		TickServerHitCheck();
	}

	if (IsLocallyControlled())
	{
		UpdateMatchCamera(DeltaSeconds);
		DrawAimPreview();
	}

	if (CVarJGDrawDebug.GetValueOnGameThread())
	{
		DrawDebugReach();
	}
}

// ----- Movement -----

void AJGCharacter::DoMove(float Right, float Forward)
{
	if (!GetController() || !CanMoveNow())
	{
		return;
	}

	// camera basis projected on the floor, so "up" on screen is always toward the opponent
	const FRotator YawRotation(0.0, MatchCamera->GetComponentRotation().Yaw, 0.0);
	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(ForwardDirection, Forward);
	AddMovementInput(RightDirection, Right);

	const FVector Desired = ForwardDirection * Forward + RightDirection * Right;
	if (Desired.SizeSquared2D() > 0.01)
	{
		LastMoveDirection = Desired.GetSafeNormal2D();
	}
}

bool AJGCharacter::CanMoveNow() const
{
	if (SlideState != EJGSlideState::Ready)
	{
		return false;
	}

	const AJGGameStateBase* JGGameState = GetJGGameState();
	if (!JGGameState)
	{
		return true;
	}

	// prototype: gated on the owning client only, the server does not enforce it yet
	switch (JGGameState->GetPhase())
	{
	case EJGMatchPhase::MatchEnd:
		return false;

	case EJGMatchPhase::PreparingServe:
	case EJGMatchPhase::Serving:
		return GetTeam() != JGGameState->GetServingTeam();

	default:
		return true;
	}
}

// ----- Attack -----

void AJGCharacter::SubmitAttack(const FJGAttackInput& Input)
{
	if (!IsLocallyControlled() || !Input.IsValid())
	{
		return;
	}

	FJGAttackInput Request = Input;
	Request.InputId = ++NextInputId;

	SetAimPreview(false, 0.0f, 0.0f);
	ServerSubmitAttack(Request);
}

void AJGCharacter::ServerSubmitAttack_Implementation(const FJGAttackInput& Input)
{
	HandleAttackInputOnServer(Input);
}

void AJGCharacter::HandleAttackInputOnServer(const FJGAttackInput& Input)
{
	const AJGGameModeBase* GameMode = GetWorld()->GetAuthGameMode<AJGGameModeBase>();
	const AJGGameStateBase* JGGameState = GetJGGameState();

	if (!GameMode || !JGGameState || !Input.IsValid() || SlideState != EJGSlideState::Ready)
	{
		return;
	}

	const UJGBalanceData* Balance = UJGBalanceData::Get();

	// never trust client values beyond the allowed ranges
	FJGAttackInput Sanitized = Input;
	Sanitized.Power = FMath::Clamp(Sanitized.Power, 0.0f, 1.0f);
	Sanitized.AimAngleDeg = FMath::Clamp(Sanitized.AimAngleDeg, -Balance->AimAngleLimitDeg, Balance->AimAngleLimitDeg);

	bool bIsServe = false;
	if (!GameMode->CanCharacterAttack(this, bIsServe))
	{
		return;
	}

	if (bIsServe)
	{
		if (AJGBall* Ball = JGGameState->GetBall())
		{
			ExecuteHit(*Ball, Sanitized, true);
		}
		return;
	}

	// single buffer slot: a new input replaces the previous one
	BufferedAttack = Sanitized;
	BufferedAttackExpireTime = GetWorld()->GetTimeSeconds() + Balance->AttackInputBufferTime;

	TryExecuteBufferedAttack();
}

void AJGCharacter::TickServerHitCheck()
{
	if (SlideState == EJGSlideState::Sliding)
	{
		TryExecuteSlideSave();
		return;
	}

	if (!BufferedAttack.IsValid())
	{
		return;
	}

	if (GetWorld()->GetTimeSeconds() > BufferedAttackExpireTime)
	{
		ClearBufferedAttack();
		return;
	}

	TryExecuteBufferedAttack();
}

bool AJGCharacter::TryExecuteBufferedAttack()
{
	if (GetWorld()->GetTimeSeconds() < NextAttackAllowedTime)
	{
		return false;
	}

	const AJGGameModeBase* GameMode = GetWorld()->GetAuthGameMode<AJGGameModeBase>();
	const AJGGameStateBase* JGGameState = GetJGGameState();
	AJGBall* Ball = JGGameState ? JGGameState->GetBall() : nullptr;

	bool bIsServe = false;
	if (!GameMode || !Ball || !GameMode->CanCharacterAttack(this, bIsServe) || bIsServe)
	{
		return false;
	}

	if (!Ball->IsHittableBy(GetTeam()))
	{
		return false;
	}

	const UJGBalanceData* Balance = UJGBalanceData::Get();
	const float ReachMultiplier = Balance->GetStatMultiplier(GetStat(&UJGCharacterData::ReachStat));

	if (!IsBallInReach(*Ball, Balance->ReachHorizontal * ReachMultiplier, Balance->ReachMinHeight, Balance->ReachMaxHeight))
	{
		return false;
	}

	ExecuteHit(*Ball, BufferedAttack, false);
	return true;
}

void AJGCharacter::ExecuteHit(AJGBall& Ball, const FJGAttackInput& Input, bool bIsServe)
{
	const AJGGameStateBase* JGGameState = GetJGGameState();
	const AJGCourt* Court = JGGameState ? JGGameState->GetCourt() : nullptr;
	AJGGameModeBase* GameMode = GetWorld()->GetAuthGameMode<AJGGameModeBase>();

	if (!Court || !GameMode)
	{
		return;
	}

	const UJGBalanceData* Balance = UJGBalanceData::Get();
	const FJGAttackInput UsedInput = Input;

	// accuracy deviation is rolled once here and replicated through the ball velocity
	const float KickMultiplier = Balance->GetStatMultiplier(GetStat(&UJGCharacterData::KickPowerStat));
	const float MaxDeviation = Balance->GetAccuracyDeviationDeg(GetStat(&UJGCharacterData::AccuracyStat));
	const float Deviation = MaxDeviation > 0.0f ? FMath::FRandRange(-MaxDeviation, MaxDeviation) : 0.0f;

	const FVector Velocity = UJGShotCalculator::ComputeAttackVelocity(*Court, GetTeam(), Ball.GetActorLocation(), UsedInput, KickMultiplier, Deviation, Ball.GetBallGravityZ());

	// consume the input first so the same ball can never be hit twice by one input
	ClearBufferedAttack();
	NextAttackAllowedTime = GetWorld()->GetTimeSeconds() + Balance->AttackRecoveryTime;

	Ball.LaunchByHit(Velocity, GetTeam());
	MulticastPlayKickMotion(UJGShotCalculator::SelectKickMotion(UsedInput, bIsServe));
	GameMode->NotifyBallHit(this, bIsServe);

	UE_LOG(LogDoJokgu, Verbose, TEXT("%s hit #%d type %d power %.2f angle %.1f serve %d"),
		*GetName(), UsedInput.InputId, static_cast<int32>(UsedInput.Type), UsedInput.Power, UsedInput.AimAngleDeg, bIsServe);
}

bool AJGCharacter::IsBallInReach(const AJGBall& Ball, float HorizontalReach, float MinHeight, float MaxHeight) const
{
	const FVector Foot = GetFootLocation();
	const FVector BallLocation = Ball.GetActorLocation();
	const double Height = BallLocation.Z - Foot.Z;

	return FVector::Dist2D(Foot, BallLocation) <= HorizontalReach && Height >= MinHeight && Height <= MaxHeight;
}

int32 AJGCharacter::GetStat(int32 UJGCharacterData::* Stat) const
{
	return CharacterData ? CharacterData->*Stat : DefaultStat;
}

void AJGCharacter::ClearBufferedAttack()
{
	BufferedAttack = FJGAttackInput();
	BufferedAttackExpireTime = 0.0;
}

void AJGCharacter::MulticastPlayKickMotion_Implementation(EJGKickMotion Motion)
{
	if (CharacterData)
	{
		if (UAnimMontage* Montage = CharacterData->GetMontageForMotion(Motion))
		{
			PlayAnimMontage(Montage);
		}
	}

	OnKickMotionPlayed(Motion);
}

// ----- Slide -----

void AJGCharacter::DoSlide()
{
	if (!IsLocallyControlled() || SlideState != EJGSlideState::Ready || !CanMoveNow())
	{
		return;
	}

	FVector Direction = LastMoveDirection.IsNearlyZero() ? GetAttackForward() : LastMoveDirection;
	Direction = Direction.GetSafeNormal2D();

	// remote clients predict the slide, the server applies the authoritative one
	if (!HasAuthority())
	{
		StartSlide(Direction);
	}

	ServerStartSlide(Direction);
}

void AJGCharacter::ServerStartSlide_Implementation(FVector_NetQuantizeNormal Direction)
{
	const AJGGameModeBase* GameMode = GetWorld()->GetAuthGameMode<AJGGameModeBase>();
	if (SlideState != EJGSlideState::Ready || !GameMode || !GameMode->CanCharacterSlide(this))
	{
		return;
	}

	FVector SlideDirection = FVector(Direction).GetSafeNormal2D();
	if (SlideDirection.IsNearlyZero())
	{
		SlideDirection = GetAttackForward();
	}

	StartSlide(SlideDirection);
	MulticastPlayKickMotion(EJGKickMotion::Slide);
}

void AJGCharacter::StartSlide(const FVector& Direction)
{
	const UJGBalanceData* Balance = UJGBalanceData::Get();

	SlideState = EJGSlideState::Sliding;
	bSlideSaveUsed = false;
	ClearBufferedAttack();

	// movement goes through CharacterMovement so it is predicted and corrected like normal walking
	TSharedPtr<FRootMotionSource_ConstantForce> Force = MakeShared<FRootMotionSource_ConstantForce>();
	Force->InstanceName = SlideRootMotionName;
	Force->AccumulateMode = ERootMotionAccumulateMode::Override;
	Force->Priority = 5;
	Force->Force = Direction * (Balance->SlideDistance / Balance->SlideActiveTime);
	Force->Duration = Balance->SlideActiveTime;
	Force->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
	Force->FinishVelocityParams.SetVelocity = FVector::ZeroVector;

	GetCharacterMovement()->ApplyRootMotionSource(Force);

	GetWorldTimerManager().SetTimer(SlideTimer, this, &AJGCharacter::EndSlideActive, Balance->SlideActiveTime, false);
}

void AJGCharacter::EndSlideActive()
{
	SlideState = EJGSlideState::Recovery;
	GetWorldTimerManager().SetTimer(SlideTimer, this, &AJGCharacter::EndSlideRecovery, UJGBalanceData::Get()->SlideRecoveryTime, false);
}

void AJGCharacter::EndSlideRecovery()
{
	SlideState = EJGSlideState::Ready;
}

void AJGCharacter::CancelSlide()
{
	GetWorldTimerManager().ClearTimer(SlideTimer);
	GetCharacterMovement()->RemoveRootMotionSource(SlideRootMotionName);
	SlideState = EJGSlideState::Ready;
	bSlideSaveUsed = false;
}

void AJGCharacter::TryExecuteSlideSave()
{
	if (bSlideSaveUsed)
	{
		return;
	}

	AJGGameModeBase* GameMode = GetWorld()->GetAuthGameMode<AJGGameModeBase>();
	const AJGGameStateBase* JGGameState = GetJGGameState();
	AJGBall* Ball = JGGameState ? JGGameState->GetBall() : nullptr;
	const AJGCourt* Court = JGGameState ? JGGameState->GetCourt() : nullptr;

	bool bIsServe = false;
	if (!GameMode || !Ball || !Court || !GameMode->CanCharacterAttack(this, bIsServe) || bIsServe || !Ball->IsHittableBy(GetTeam()))
	{
		return;
	}

	// widened reach only while sliding
	const UJGBalanceData* Balance = UJGBalanceData::Get();
	const float ReachMultiplier = Balance->GetStatMultiplier(GetStat(&UJGCharacterData::ReachStat));

	if (!IsBallInReach(*Ball, Balance->SlideReachHorizontal * ReachMultiplier, Balance->ReachMinHeight, Balance->SlideReachMaxHeight))
	{
		return;
	}

	const FVector Velocity = UJGShotCalculator::ComputeSlideReturnVelocity(*Court, GetTeam(), Ball->GetActorLocation(), Ball->GetBallGravityZ());

	bSlideSaveUsed = true;
	Ball->LaunchByHit(Velocity, GetTeam());
	GameMode->NotifyBallHit(this, false);
}

// ----- Point reset -----

void AJGCharacter::ResetForNewPoint()
{
	CancelSlide();
	ClearBufferedAttack();
	NextAttackAllowedTime = 0.0;

	if (!IsLocallyControlled())
	{
		ClientResetForNewPoint();
	}
}

void AJGCharacter::ClientResetForNewPoint_Implementation()
{
	CancelSlide();
	SetAimPreview(false, 0.0f, 0.0f);
}

// ----- Data -----

void AJGCharacter::SetCharacterData(UJGCharacterData* NewData)
{
	CharacterData = NewData;
	ApplyCharacterData();
}

void AJGCharacter::OnRep_CharacterData()
{
	ApplyCharacterData();
}

void AJGCharacter::ApplyCharacterData()
{
	const UJGBalanceData* Balance = UJGBalanceData::Get();
	GetCharacterMovement()->MaxWalkSpeed = Balance->BaseMoveSpeed * Balance->GetStatMultiplier(GetStat(&UJGCharacterData::MoveSpeedStat));

	if (!CharacterData)
	{
		return;
	}

	if (CharacterData->SkeletalMesh)
	{
		GetMesh()->SetSkeletalMeshAsset(CharacterData->SkeletalMesh);
	}

	if (CharacterData->AnimClass)
	{
		GetMesh()->SetAnimInstanceClass(CharacterData->AnimClass);
	}
}

// ----- Queries -----

EJGTeam AJGCharacter::GetTeam() const
{
	const AJGPlayerState* JGPlayerState = GetPlayerState<AJGPlayerState>();
	return JGPlayerState ? JGPlayerState->GetTeam() : EJGTeam::None;
}

FVector AJGCharacter::GetAttackForward() const
{
	const AJGGameStateBase* JGGameState = GetJGGameState();
	const AJGCourt* Court = JGGameState ? JGGameState->GetCourt() : nullptr;

	if (!Court)
	{
		return GetActorForwardVector();
	}

	const EJGTeam Team = GetTeam();
	return Court->GetAttackForward(Team == EJGTeam::None ? EJGTeam::A : Team);
}

FVector AJGCharacter::GetFootLocation() const
{
	return GetActorLocation() - FVector(0.0, 0.0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
}

AJGGameStateBase* AJGCharacter::GetJGGameState() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetGameState<AJGGameStateBase>() : nullptr;
}

// ----- Presentation -----

void AJGCharacter::SetAimPreview(bool bVisible, float AimAngleDeg, float Power)
{
	bAimPreviewVisible = bVisible;
	AimPreviewAngle = AimAngleDeg;
	AimPreviewPower = Power;

	OnAimPreviewChanged(bVisible, UJGShotCalculator::GetAimDirection(GetAttackForward(), AimAngleDeg), Power);
}

void AJGCharacter::UpdateMatchCamera(float DeltaSeconds)
{
	const AJGGameStateBase* JGGameState = GetJGGameState();
	const AJGCourt* Court = JGGameState ? JGGameState->GetCourt() : nullptr;
	if (!Court)
	{
		return;
	}

	EJGTeam Team = GetTeam();
	if (Team == EJGTeam::None)
	{
		Team = EJGTeam::A;
	}

	const FTransform Target = Court->GetCameraViewTransform(Team, GetActorLocation());

	// snap when the team becomes known, otherwise follow smoothly
	if (!bCameraInitialized || CameraTeam != Team)
	{
		bCameraInitialized = true;
		CameraTeam = Team;
		MatchCamera->SetWorldLocationAndRotation(Target.GetLocation(), Target.GetRotation());
		return;
	}

	const float InterpSpeed = UJGBalanceData::Get()->CameraFollowInterpSpeed;
	const FVector Location = FMath::VInterpTo(MatchCamera->GetComponentLocation(), Target.GetLocation(), DeltaSeconds, InterpSpeed);
	const FRotator Rotation = FMath::RInterpTo(MatchCamera->GetComponentRotation(), Target.Rotator(), DeltaSeconds, InterpSpeed);
	MatchCamera->SetWorldLocationAndRotation(Location, Rotation);
}

void AJGCharacter::DrawAimPreview() const
{
#if ENABLE_DRAW_DEBUG
	if (!bAimPreviewVisible || !bDrawPlaceholderAimArrow)
	{
		return;
	}

	const UJGBalanceData* Balance = UJGBalanceData::Get();
	const FVector Direction = UJGShotCalculator::GetAimDirection(GetAttackForward(), AimPreviewAngle);
	const FVector Start = GetFootLocation() + FVector(0.0, 0.0, 10.0);
	const double Length = 80.0 + 220.0 * AimPreviewPower;

	FColor Color = FColor::Green;
	if (AimPreviewPower >= Balance->StrongMotionMinPower)
	{
		Color = FColor::Red;
	}
	else if (AimPreviewPower >= Balance->WeakMotionMaxPower)
	{
		Color = FColor::Yellow;
	}

	DrawDebugDirectionalArrow(GetWorld(), Start, Start + Direction * Length, 40.0f, Color, false, -1.0f, 0, 6.0f);
#endif
}

void AJGCharacter::DrawDebugReach() const
{
#if ENABLE_DRAW_DEBUG
	const UJGBalanceData* Balance = UJGBalanceData::Get();
	const float ReachMultiplier = Balance->GetStatMultiplier(GetStat(&UJGCharacterData::ReachStat));
	const bool bSliding = SlideState == EJGSlideState::Sliding;
	const float Reach = (bSliding ? Balance->SlideReachHorizontal : Balance->ReachHorizontal) * ReachMultiplier;
	const float MaxHeight = bSliding ? Balance->SlideReachMaxHeight : Balance->ReachMaxHeight;
	const FVector Foot = GetFootLocation();
	const FColor Color = bSliding ? FColor::Orange : FColor::Green;

	DrawDebugCircle(GetWorld(), Foot + FVector(0.0, 0.0, 2.0), Reach, 32, Color, false, -1.0f, 0, 2.0f, FVector::ForwardVector, FVector::RightVector, false);
	DrawDebugCircle(GetWorld(), Foot + FVector(0.0, 0.0, MaxHeight), Reach, 32, Color, false, -1.0f, 0, 1.0f, FVector::ForwardVector, FVector::RightVector, false);
#endif
}
