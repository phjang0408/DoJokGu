#include "Jokgu/Player/JGCharacter.h"
#include "Jokgu/Player/JGCharacterMovementComponent.h"
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
	/** 슬라이딩 RootMotionSource 이름. 취소할 때 이 이름으로 찾아서 지운다. */
	const FName SlideRootMotionName(TEXT("JGSlide"));

	/** 캐릭터 데이터가 없을 때 쓰는 능력치 (배율 100%) */
	constexpr int32 DefaultStat = 3;
}

#pragma region 초기화와 입력 바인딩

AJGCharacter::AJGCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UJGCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

	// 캐릭터는 컨트롤러 회전이 아니라 이동 방향을 바라본다
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

	// 카메라는 캐릭터를 따라 움직이지 않고 UpdateMatchCamera가 월드 위치를 직접 정한다
	MatchCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("MatchCamera"));
	MatchCamera->SetupAttachment(RootComponent);
	MatchCamera->SetUsingAbsoluteLocation(true);
	MatchCamera->SetUsingAbsoluteRotation(true);
	MatchCamera->bUsePawnControlRotation = false;

	// 참고: 스켈레탈 메시와 애님 블루프린트는 BP_JGCharacter 또는 UJGCharacterData에서 지정한다
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

#pragma endregion

#pragma region 이동

void AJGCharacter::DoMove(float Right, float Forward)
{
	if (!GetController() || !CanMoveNow())
	{
		return;
	}

	// 카메라 축을 바닥에 투영해서 쓰므로 화면 "위"는 언제나 상대 쪽이다
	const FRotator YawRotation(0.0, MatchCamera->GetComponentRotation().Yaw, 0.0);
	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(ForwardDirection, Forward);
	AddMovementInput(RightDirection, Right);

	// 아주 작은 스틱 입력은 슬라이딩 방향으로 기억하지 않는다
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

	// 서버도 UJGCharacterMovementComponent를 통해 같은 규칙을 강제한다
	switch (JGGameState->GetPhase())
	{
	case EJGMatchPhase::MatchEnd:
		return false;

	// 서브하는 쪽은 서브 위치에 고정, 받는 쪽은 움직일 수 있다
	case EJGMatchPhase::PreparingServe:
	case EJGMatchPhase::Serving:
		return GetTeam() != JGGameState->GetServingTeam();

	default:
		return true;
	}
}

#pragma endregion

#pragma region 공격

void AJGCharacter::SubmitAttack(const FJGAttackInput& Input)
{
	if (!IsLocallyControlled() || !Input.IsValid())
	{
		return;
	}

	FJGAttackInput Request = Input;
	Request.InputId = ++NextInputId;

	// 서버가 거절했을 때 비교할 수 있게 지금 화면에 보이는 공의 위치를 함께 보낸다
	const AJGGameStateBase* JGGameState = GetJGGameState();
	if (const AJGBall* Ball = JGGameState ? JGGameState->GetBall() : nullptr)
	{
		MeasureBall(Ball->GetVisualLocation(), Request.ClientBallDistance, Request.ClientBallHeight);
	}

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

	if (!GameMode || !JGGameState || !Input.IsValid())
	{
		return;
	}

	++AttackRequestCount;

	const AJGBall* CurrentBall = JGGameState->GetBall();
	float ServerDistance = -1.0f;
	float ServerHeight = 0.0f;
	if (CurrentBall)
	{
		MeasureBall(CurrentBall->GetActorLocation(), ServerDistance, ServerHeight);
	}

	if (SlideState != EJGSlideState::Ready)
	{
		LogRejectedAttack(Input, EHitReject::Sliding, ServerDistance, ServerHeight);
		return;
	}

	const UJGBalanceData* Balance = UJGBalanceData::Get();

	// 클라이언트 값은 허용 범위 밖이면 믿지 않고 잘라 낸다
	FJGAttackInput Sanitized = Input;
	Sanitized.Power = FMath::Clamp(Sanitized.Power, 0.0f, 1.0f);
	Sanitized.AimAngleDeg = FMath::Clamp(Sanitized.AimAngleDeg, -Balance->AimAngleLimitDeg, Balance->AimAngleLimitDeg);

	bool bIsServe = false;
	if (!GameMode->CanCharacterAttack(this, bIsServe))
	{
		LogRejectedAttack(Input, EHitReject::Phase, ServerDistance, ServerHeight);
		return;
	}

	// 서브는 공을 들고 있으므로 범위 검사 없이 바로 친다
	if (bIsServe)
	{
		if (AJGBall* Ball = JGGameState->GetBall())
		{
			ExecuteHit(*Ball, Sanitized, true);
		}
		return;
	}

	// 저장 슬롯은 하나: 새 입력이 이전 입력을 대신한다. 대신된 입력은 실행되지 못한 것이므로 기록한다.
	if (BufferedAttack.IsValid())
	{
		LogRejectedAttack(BufferedAttack, BufferedReject, BufferedRejectDistance, BufferedRejectHeight);
	}

	BufferedAttack = Sanitized;
	BufferedReject = EHitReject::None;
	BufferedAttackExpireTime = GetWorld()->GetTimeSeconds() + Balance->AttackInputBufferTime;

	TryExecuteBufferedAttack();
}

void AJGCharacter::TickServerHitCheck()
{
	// 슬라이딩 중에는 일반 공격 대신 자동 수비 타격만 검사한다
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
		LogRejectedAttack(BufferedAttack, BufferedReject, BufferedRejectDistance, BufferedRejectHeight);
		ClearBufferedAttack();
		return;
	}

	TryExecuteBufferedAttack();
}

bool AJGCharacter::TryExecuteBufferedAttack()
{
	const AJGGameModeBase* GameMode = GetWorld()->GetAuthGameMode<AJGGameModeBase>();
	const AJGGameStateBase* JGGameState = GetJGGameState();
	AJGBall* Ball = JGGameState ? JGGameState->GetBall() : nullptr;

	// 실행하지 못하면 이유와 그때의 공 위치를 남겨 두고, 입력이 만료될 때 로그에 쓴다
	if (Ball)
	{
		MeasureBall(Ball->GetActorLocation(), BufferedRejectDistance, BufferedRejectHeight);
	}

	if (GetWorld()->GetTimeSeconds() < NextAttackAllowedTime)
	{
		BufferedReject = EHitReject::Recovery;
		return false;
	}

	// 저장된 입력은 랠리 공격 전용이다. 그사이 서브 단계가 됐으면 실행하지 않는다.
	bool bIsServe = false;
	if (!GameMode || !Ball || !GameMode->CanCharacterAttack(this, bIsServe) || bIsServe)
	{
		BufferedReject = EHitReject::Phase;
		return false;
	}

	if (!Ball->IsHittableBy(GetTeam()))
	{
		const bool bRallyLive = Ball->IsInPlay() && Ball->GetRallyState().LastHitterTeam != EJGTeam::None;
		BufferedReject = bRallyLive ? EHitReject::NotYourTurn : EHitReject::RallyOver;
		return false;
	}

	const UJGBalanceData* Balance = UJGBalanceData::Get();

	if (BufferedRejectDistance > GetAttackReach())
	{
		BufferedReject = EHitReject::TooFar;
		return false;
	}

	if (BufferedRejectHeight < Balance->ReachMinHeight || BufferedRejectHeight > Balance->ReachMaxHeight)
	{
		BufferedReject = BufferedRejectHeight < Balance->ReachMinHeight ? EHitReject::TooLow : EHitReject::TooHigh;
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
	// Input이 BufferedAttack을 가리킬 수 있어서 지우기 전에 복사해 둔다
	const FJGAttackInput UsedInput = Input;

	// 정확도 오차는 여기서 한 번 뽑고, 공 속도를 통해 복제된다
	const float KickMultiplier = Balance->GetStatMultiplier(GetStat(&UJGCharacterData::KickPowerStat));
	const float MaxDeviation = Balance->GetAccuracyDeviationDeg(GetStat(&UJGCharacterData::AccuracyStat));
	const float Deviation = MaxDeviation > 0.0f ? FMath::FRandRange(-MaxDeviation, MaxDeviation) : 0.0f;

	const FVector Velocity = UJGShotCalculator::ComputeAttackVelocity(*Court, GetTeam(), Ball.GetActorLocation(), UsedInput, KickMultiplier, Deviation, Ball.GetBallGravityZ());

	// 입력을 먼저 소비해서 입력 하나로 같은 공을 두 번 치는 일이 없게 한다
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
	float Distance = 0.0f;
	float Height = 0.0f;
	MeasureBall(Ball.GetActorLocation(), Distance, Height);

	return Distance <= HorizontalReach && Height >= MinHeight && Height <= MaxHeight;
}

float AJGCharacter::GetAttackReach() const
{
	const UJGBalanceData* Balance = UJGBalanceData::Get();
	return Balance->ReachHorizontal * Balance->GetStatMultiplier(GetStat(&UJGCharacterData::ReachStat));
}

void AJGCharacter::MeasureBall(const FVector& BallLocation, float& OutDistance, float& OutHeight) const
{
	const FVector Foot = GetFootLocation();
	OutDistance = static_cast<float>(FVector::Dist2D(Foot, BallLocation));
	OutHeight = static_cast<float>(BallLocation.Z - Foot.Z);
}

int32 AJGCharacter::GetStat(int32 UJGCharacterData::* Stat) const
{
	return CharacterData ? CharacterData->*Stat : DefaultStat;
}

void AJGCharacter::ClearBufferedAttack()
{
	BufferedAttack = FJGAttackInput();
	BufferedAttackExpireTime = 0.0;
	BufferedReject = EHitReject::None;
}

void AJGCharacter::MulticastPlayKickMotion_Implementation(EJGKickMotion Motion)
{
	if (CharacterData)
	{
		if (UAnimMontage* Montage = CharacterData->GetMontageForMotion(Motion))
		{
			// 서버가 이미 공을 발사했으므로 확정된 타격은 준비 동작을 건너뛰고 Contact 구간부터 재생한다.
			// 슬라이딩은 접촉 전에 요청되므로 시작/반복/회복 전체를 재생해야 한다.
			const FName StartSection = Motion != EJGKickMotion::Slide && Montage->IsValidSectionName(TEXT("Contact"))
				? FName(TEXT("Contact")) : NAME_None;
			PlayAnimMontage(Montage, 1.f, StartSection);
		}
	}

	OnKickMotionPlayed(Motion);
}

#pragma endregion

#pragma region 타격 거절 기록

void AJGCharacter::LogRejectedAttack(const FJGAttackInput& Input, EHitReject Reason, float ServerDistance, float ServerHeight)
{
	++RejectedAttackCount;

	const UJGBalanceData* Balance = UJGBalanceData::Get();
	const float Reach = GetAttackReach();

	// 클라이언트 화면에서는 공이 리치 안에 있었는데 서버가 거절했다면 지연 때문일 가능성이 크다
	const bool bInReachOnScreen = Input.ClientBallDistance >= 0.0f && Input.ClientBallDistance <= Reach
		&& Input.ClientBallHeight >= Balance->ReachMinHeight && Input.ClientBallHeight <= Balance->ReachMaxHeight;

	const AJGPlayerState* JGPlayerState = GetPlayerState<AJGPlayerState>();
	const AJGGameStateBase* JGGameState = GetJGGameState();
	const AJGBall* Ball = JGGameState ? JGGameState->GetBall() : nullptr;

	UE_LOG(LogDoJokgu, Log,
		TEXT("HitReject %s input #%d (shot %d): %s | server ball %.0f cm away, %.0f cm high | screen ball %.0f cm, %.0f cm%s | reach %.0f, height %.0f~%.0f | ping %.0f ms | rejected %d of %d"),
		*GetNameSafe(JGPlayerState), Input.InputId, Ball ? Ball->GetRallyState().ShotId : 0, HitRejectName(Reason),
		ServerDistance, ServerHeight, Input.ClientBallDistance, Input.ClientBallHeight,
		bInReachOnScreen ? TEXT(" (IN REACH ON SCREEN)") : TEXT(""),
		Reach, Balance->ReachMinHeight, Balance->ReachMaxHeight,
		JGPlayerState ? JGPlayerState->GetPingInMilliseconds() : 0.0f,
		RejectedAttackCount, AttackRequestCount);
}

const TCHAR* AJGCharacter::HitRejectName(EHitReject Reason)
{
	switch (Reason)
	{
	case EHitReject::Phase: return TEXT("phase does not take attacks");
	case EHitReject::Sliding: return TEXT("sliding or recovering");
	case EHitReject::Recovery: return TEXT("recovering from last hit");
	case EHitReject::RallyOver: return TEXT("rally over");
	case EHitReject::NotYourTurn: return TEXT("not your turn to receive");
	case EHitReject::TooFar: return TEXT("ball too far");
	case EHitReject::TooLow: return TEXT("ball too low");
	case EHitReject::TooHigh: return TEXT("ball too high");
	default: return TEXT("ball never checked");
	}
}

#pragma endregion

#pragma region 슬라이딩

void AJGCharacter::DoSlide()
{
	if (!IsLocallyControlled() || SlideState != EJGSlideState::Ready || !CanMoveNow())
	{
		return;
	}

	FVector Direction = LastMoveDirection.IsNearlyZero() ? GetAttackForward() : LastMoveDirection;
	Direction = Direction.GetSafeNormal2D();

	// 원격 클라이언트는 슬라이딩을 예측 실행하고, 서버가 권한 있는 슬라이딩을 적용한다
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

	// 이동은 CharacterMovement를 거치므로 일반 걷기처럼 예측과 보정을 받는다.
	// 일정한 속도로 SlideActiveTime 동안 SlideDistance만큼 미끄러지고 끝나면 멈춘다.
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

	// 슬라이딩 중에만 넓어지는 리치
	const UJGBalanceData* Balance = UJGBalanceData::Get();
	const float ReachMultiplier = Balance->GetStatMultiplier(GetStat(&UJGCharacterData::ReachStat));

	if (!IsBallInReach(*Ball, Balance->SlideReachHorizontal * ReachMultiplier, Balance->ReachMinHeight, Balance->SlideReachMaxHeight))
	{
		return;
	}

	const FVector Velocity = UJGShotCalculator::ComputeSlideReturnVelocity(*Court, GetTeam(), Ball->GetActorLocation(), Ball->GetBallGravityZ());

	// 슬라이딩 수비는 킥 몽타주를 따로 재생하지 않고, 슬라이딩 시작 때 재생한 몽타주를 그대로 이어 간다
	bSlideSaveUsed = true;
	Ball->LaunchByHit(Velocity, GetTeam());
	GameMode->NotifyBallHit(this, false);
}

#pragma endregion

#pragma region 득점 후 초기화

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

#pragma endregion

#pragma region 캐릭터 데이터

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
	// 이동 속도는 서버와 클라이언트가 같아야 이동 예측이 어긋나지 않으므로 양쪽에서 적용한다
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

#pragma endregion

#pragma region 조회

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

	// 팀이 아직 복제되지 않았으면 A팀 기준으로 본다
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

#pragma endregion

#pragma region 표시

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

	// 팀이 정해지는 순간에는 바로 옮기고, 그 뒤에는 부드럽게 따라간다
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

	// 화살표 길이는 파워에 비례하고, 색은 재생될 킥 동작 구간(약/중/강)을 나타낸다
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
	// 발 높이와 최대 타격 높이에 리치 원을 그린다. 초록 = 평소, 주황 = 슬라이딩 중.
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

#pragma endregion
