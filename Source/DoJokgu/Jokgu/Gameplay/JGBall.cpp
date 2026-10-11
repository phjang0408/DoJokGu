#include "Jokgu/Gameplay/JGBall.h"
#include "DoJokgu.h"
#include "Jokgu/Core/JGDebug.h"
#include "Jokgu/Core/JGMatchRules.h"
#include "Jokgu/Data/JGBalanceData.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Net/UnrealNetwork.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

#pragma region 초기화

AJGBall::AJGBall()
{
	PrimaryActorTick.bCanEverTick = true;

	// 공은 화면 어디서나 보여야 하므로 거리와 상관없이 자주 복제한다
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(true);
	SetNetUpdateFrequency(60.0f);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	// JGBall 채널로 쿼리만 하고, WorldStatic(코트 바닥·네트)만 막는다
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(13.0f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_JGBall);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Collision->SetCanEverAffectNavigation(false);
	RootComponent = Collision;

	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(Collision);
	VisualMesh->SetStaticMesh(SphereMesh.Object);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMesh->SetCanEverAffectNavigation(false);

	// 그림자는 월드 기준으로 직접 놓으므로 부모 트랜스폼을 따르지 않는다
	ShadowMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShadowMesh"));
	ShadowMesh->SetupAttachment(Collision);
	ShadowMesh->SetStaticMesh(CylinderMesh.Object);
	ShadowMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ShadowMesh->SetCanEverAffectNavigation(false);
	ShadowMesh->SetCastShadow(false);
	ShadowMesh->SetUsingAbsoluteLocation(true);
	ShadowMesh->SetUsingAbsoluteRotation(true);
	ShadowMesh->SetUsingAbsoluteScale(true);

	// 빠른 공이 얇은 네트를 뚫지 않도록 120Hz로 나눠 스윕한다
	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->SetUpdatedComponent(Collision);
	Movement->bAutoActivate = false;
	Movement->InitialSpeed = 0.0f;
	Movement->MaxSpeed = 0.0f;
	Movement->bShouldBounce = true;
	Movement->bRotationFollowsVelocity = false;
	Movement->bSweepCollision = true;
	Movement->bForceSubStepping = true;
	Movement->MaxSimulationTimeStep = 1.0f / 120.0f;
	Movement->MaxSimulationIterations = 8;
	Movement->BounceVelocityStopSimulatingThreshold = 40.0f;
}

void AJGBall::BeginPlay()
{
	Super::BeginPlay();

	ApplyBalance();

	// 엔진 기본 도형은 BasicShapeMaterial을 쓰고, 이 재질에 "Color" 파라미터가 있다
	if (UMaterialInstanceDynamic* Material = VisualMesh->CreateDynamicMaterialInstance(0))
	{
		Material->SetVectorParameterValue(TEXT("Color"), BallColor);
	}

	if (UMaterialInstanceDynamic* Material = ShadowMesh->CreateDynamicMaterialInstance(0))
	{
		Material->SetVectorParameterValue(TEXT("Color"), ShadowColor);
	}

	Movement->OnProjectileBounce.AddDynamic(this, &AJGBall::HandleBounce);
	Movement->OnProjectileStop.AddDynamic(this, &AJGBall::HandleStop);
}

void AJGBall::ApplyBalance()
{
	const UJGBalanceData* Balance = UJGBalanceData::Get();

	Collision->SetSphereRadius(Balance->BallRadius);
	// 엔진 기본 구 메시는 지름 100cm
	VisualMesh->SetRelativeScale3D(FVector(VisualRadius * 2.0f / 100.0f));

	Movement->ProjectileGravityScale = Balance->BallGravityScale;
	Movement->Bounciness = Balance->BallBounciness;
	Movement->Friction = Balance->BallFriction;
}

void AJGBall::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AJGBall, bInPlay);
	DOREPLIFETIME(AJGBall, RallyState);
}

#pragma endregion

#pragma region 틱

void AJGBall::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority())
	{
		UpdateNetCrossing();
	}
	else
	{
		UpdateVisualSmoothing(DeltaSeconds);
	}

	UpdateShadow();

	if (HasAuthority() && bInPlay && !bOutOfPlayReported)
	{
		// 안전장치: 바닥에 닿지 않고 경기 영역 밖으로 떨어진 공도 랠리를 끝낸다
		if (GetActorLocation().Z < UJGBalanceData::Get()->OutOfPlayZ)
		{
			bOutOfPlayReported = true;
			OnGroundContact.Broadcast(this, GetActorLocation());
		}
	}

	if (CVarJGDrawDebug.GetValueOnGameThread())
	{
		DrawDebugTrajectory();
	}
}

#pragma endregion

#pragma region 클라이언트 보정과 표시 보간

void AJGBall::PostNetReceiveVelocity(const FVector& NewVelocity)
{
	Super::PostNetReceiveVelocity(NewVelocity);

	// 클라이언트 외삽이 서버 속도와 어긋나지 않게 맞춘다
	Movement->Velocity = NewVelocity;
}

void AJGBall::PostNetReceiveLocationAndRotation()
{
	// Super가 충돌체를 서버 위치로 옮기기 전에 지금 보이는 위치를 기억한다
	const FVector OldVisualLocation = VisualMesh->GetComponentLocation();

	Super::PostNetReceiveLocationAndRotation();

	if (bSnapNextNetUpdate)
	{
		bSnapNextNetUpdate = false;
		SnapVisual();
		return;
	}

	// 보이는 메시는 원래 자리에 두고, UpdateVisualSmoothing이 보정된 위치로 끌어당기게 한다
	const FVector Offset = OldVisualLocation - GetActorLocation();
	if (Offset.Size() > VisualSnapDistance)
	{
		SnapVisual();
		return;
	}

	VisualMesh->SetRelativeLocation(GetActorTransform().InverseTransformVectorNoScale(Offset));
}

void AJGBall::UpdateVisualSmoothing(float DeltaSeconds)
{
	const FVector Offset = VisualMesh->GetRelativeLocation();
	if (Offset.IsNearlyZero(0.01))
	{
		return;
	}

	// 지수 감쇠: 프레임 속도와 상관없이 VisualSmoothingTime마다 오프셋이 약 37%로 줄어든다
	const float Keep = VisualSmoothingTime > UE_KINDA_SMALL_NUMBER ? FMath::Exp(-DeltaSeconds / VisualSmoothingTime) : 0.0f;
	VisualMesh->SetRelativeLocation(Offset * Keep);
}

void AJGBall::SnapVisual()
{
	VisualMesh->SetRelativeLocation(FVector::ZeroVector);
}

void AJGBall::OnRep_RallyState()
{
	// 새 샷이나 초기화: 이전 보정을 버려서 이전 궤적이 새 궤적을 끌고 가지 않게 한다
	if (RallyState.ShotId != LastSeenShotId)
	{
		LastSeenShotId = RallyState.ShotId;
		bSnapNextNetUpdate = true;
		SnapVisual();
	}
}

#pragma endregion

#pragma region 네트 통과 기록

void AJGBall::SetCourtFrame(const FTransform& InCourtFrame)
{
	CourtFrame = InCourtFrame;
	bHasCourtFrame = true;
	PreviousCourtLocal = CourtFrame.InverseTransformPosition(GetActorLocation());
}

void AJGBall::UpdateNetCrossing()
{
	if (!bHasCourtFrame)
	{
		return;
	}

	const FVector CurrentCourtLocal = CourtFrame.InverseTransformPosition(GetActorLocation());

	if (bInPlay)
	{
		const UJGBalanceData* Balance = UJGBalanceData::Get();
		if (UJGMatchRules::RegisterNetCrossing(RallyState, PreviousCourtLocal, CurrentCourtLocal, Balance->NetHeight, Balance->BallRadius))
		{
			UE_LOG(LogDoJokgu, Verbose, TEXT("Shot %d crossed the net: clearance %.1f cm, lateral %.1f cm"),
				RallyState.ShotId, RallyState.NetCrossingClearance, RallyState.NetCrossingLateral);

#if ENABLE_DRAW_DEBUG
			// 초록 = 깨끗하게 통과, 빨강 = 네트 접촉
			if (CVarJGDrawDebug.GetValueOnGameThread())
			{
				const FColor Color = RallyState.NetCrossingClearance >= 0.0f ? FColor::Green : FColor::Red;
				DrawDebugSphere(GetWorld(), GetActorLocation(), 12.0f, 8, Color, false, 3.0f);
			}
#endif
		}
	}

	PreviousCourtLocal = CurrentCourtLocal;
}

#pragma endregion

#pragma region 서버 전용 제어

void AJGBall::HoldAt(const FVector& Location)
{
	check(HasAuthority());

	UJGMatchRules::ClearRally(RallyState);
	bOutOfPlayReported = false;
	bInPlay = false;

	SetMovementActive(false);
	SetActorLocation(Location, false, nullptr, ETeleportType::ResetPhysics);

	// 순간이동을 네트 통과로 오인하지 않도록 이전 위치도 함께 옮긴다
	if (bHasCourtFrame)
	{
		PreviousCourtLocal = CourtFrame.InverseTransformPosition(Location);
	}
	ForceNetUpdate();
}

void AJGBall::LaunchByHit(const FVector& Velocity, EJGTeam HitterTeam)
{
	check(HasAuthority());

	UJGMatchRules::RegisterHit(RallyState, HitterTeam);
	bOutOfPlayReported = false;
	bInPlay = true;
	LastGroundContactTime = -1.0;

	Movement->Velocity = Velocity;
	SetMovementActive(true);
	ForceNetUpdate();
}

void AJGBall::ClearRally()
{
	check(HasAuthority());

	UJGMatchRules::ClearRally(RallyState);
	ForceNetUpdate();
}

#pragma endregion

#pragma region 조회

bool AJGBall::IsHittableBy(EJGTeam Team) const
{
	return bInPlay && UJGMatchRules::CanTeamHit(RallyState, Team);
}

float AJGBall::GetBallGravityZ() const
{
	return Movement->GetGravityZ();
}

FVector AJGBall::GetBallVelocity() const
{
	return Movement->Velocity;
}

#pragma endregion

#pragma region 이동과 접촉 이벤트

void AJGBall::SetMovementActive(bool bActive)
{
	if (bActive)
	{
		// OnProjectileStop이 UpdatedComponent를 비우므로 발사할 때마다 다시 지정한다
		Movement->SetUpdatedComponent(Collision);
		Movement->Activate(false);
		Movement->UpdateComponentVelocity();
	}
	else
	{
		Movement->StopMovementImmediately();
		Movement->Deactivate();
	}
}

void AJGBall::OnRep_InPlay()
{
	// 들기/발사가 바뀌면 클라이언트도 이동을 켜고 끄며, 순간이동이므로 보간하지 않는다
	SetMovementActive(bInPlay);
	bSnapNextNetUpdate = true;
	SnapVisual();
}

void AJGBall::HandleBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity)
{
	if (!HasAuthority() || !bInPlay)
	{
		return;
	}

	const UPrimitiveComponent* HitComponent = ImpactResult.GetComponent();
	if (!HitComponent || !HitComponent->ComponentHasTag(JGTags::Ground()))
	{
		// 네트와 다른 차단물은 공을 튕기기만 한다
		return;
	}

	// 한 번의 착지가 서브스텝에서 여러 번 보고되는 것을 거른다
	const double Now = GetWorld()->GetTimeSeconds();
	if (LastGroundContactTime >= 0.0 && Now - LastGroundContactTime < MinGroundContactInterval)
	{
		return;
	}

	LastGroundContactTime = Now;

#if ENABLE_DRAW_DEBUG
	if (CVarJGDrawDebug.GetValueOnGameThread())
	{
		DrawDebugSphere(GetWorld(), ImpactResult.ImpactPoint, 15.0f, 8, FColor::Orange, false, 3.0f);
	}
#endif

	OnGroundContact.Broadcast(this, GetActorLocation());
}

void AJGBall::HandleStop(const FHitResult& ImpactResult)
{
	if (!HasAuthority() || !bInPlay)
	{
		return;
	}

	OnStopped.Broadcast(this, GetActorLocation());
}

#pragma endregion

#pragma region 그림자와 디버그 표시

void AJGBall::UpdateShadow()
{
	const UWorld* World = GetWorld();
	// 보정된 충돌체가 아니라 부드럽게 따라가는 보이는 공을 따라간다
	const FVector Start = VisualMesh->GetComponentLocation();
	const FVector End = Start - FVector(0.0, 0.0, 5000.0);

	// 공과 같은 JGBall 채널로 쏴서 공이 튕기는 면(코트 바닥)에만 그림자를 놓는다
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(JGBallShadow), false, this);
	const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_JGBall, Params);

	ShadowMesh->SetVisibility(bHit);
	if (!bHit)
	{
		return;
	}

	// 높이가 올라갈수록 그림자를 줄여서 높이 단서를 준다 (0-400cm에서 100%-50%)
	const double Height = Start.Z - Hit.ImpactPoint.Z;
	const double Scale = FMath::GetMappedRangeValueClamped(TRange<double>(0.0, 400.0), TRange<double>(1.0, 0.5), Height);
	const double Diameter = VisualRadius * 2.0 * Scale;

	// 바닥과 겹쳐 깜빡이지 않도록 살짝 띄운다
	ShadowMesh->SetWorldLocation(Hit.ImpactPoint + FVector(0.0, 0.0, 1.5));
	ShadowMesh->SetWorldScale3D(FVector(Diameter / 100.0, Diameter / 100.0, 0.01));
}

void AJGBall::DrawDebugTrajectory() const
{
#if ENABLE_DRAW_DEBUG
	if (!bInPlay)
	{
		return;
	}

	// 현재 속도로 최대 3초 앞까지의 포물선을 그린다 (반발과 충돌은 고려하지 않음)
	const double GravityZ = GetBallGravityZ();
	FVector Position = GetActorLocation();
	FVector Velocity = Movement->Velocity;
	const double Step = 1.0 / 30.0;

	for (int32 Index = 0; Index < 90 && Position.Z > 0.0; ++Index)
	{
		const FVector Next = Position + Velocity * Step + FVector(0.0, 0.0, 0.5 * GravityZ * Step * Step);
		DrawDebugLine(GetWorld(), Position, Next, FColor::Cyan, false, -1.0f, 0, 1.5f);
		Velocity.Z += GravityZ * Step;
		Position = Next;
	}

	DrawDebugString(GetWorld(), GetActorLocation() + FVector(0, 0, 40),
		FString::Printf(TEXT("Shot %d  hitter %d  bounce %d"), RallyState.ShotId, static_cast<int32>(RallyState.LastHitterTeam), RallyState.ReceiverBounceCount),
		nullptr, FColor::White, 0.0f);
#endif
}

#pragma endregion
