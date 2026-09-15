#include "Jokgu/Gameplay/JGBall.h"
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

AJGBall::AJGBall()
{
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(true);
	SetNetUpdateFrequency(60.0f);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

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

	ShadowMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShadowMesh"));
	ShadowMesh->SetupAttachment(Collision);
	ShadowMesh->SetStaticMesh(CylinderMesh.Object);
	ShadowMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ShadowMesh->SetCanEverAffectNavigation(false);
	ShadowMesh->SetCastShadow(false);
	ShadowMesh->SetUsingAbsoluteLocation(true);
	ShadowMesh->SetUsingAbsoluteRotation(true);
	ShadowMesh->SetUsingAbsoluteScale(true);

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

	// engine basic shapes use BasicShapeMaterial, which exposes a "Color" parameter
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

void AJGBall::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateShadow();

	if (HasAuthority() && bInPlay && !bOutOfPlayReported)
	{
		// safety net: a ball that leaves the play area without touching the floor still ends the rally
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

void AJGBall::PostNetReceiveVelocity(const FVector& NewVelocity)
{
	Super::PostNetReceiveVelocity(NewVelocity);

	// keep client extrapolation in sync with the server velocity
	Movement->Velocity = NewVelocity;
}

void AJGBall::HoldAt(const FVector& Location)
{
	check(HasAuthority());

	UJGMatchRules::ClearRally(RallyState);
	bOutOfPlayReported = false;
	bInPlay = false;

	SetMovementActive(false);
	SetActorLocation(Location, false, nullptr, ETeleportType::ResetPhysics);
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

void AJGBall::SetMovementActive(bool bActive)
{
	if (bActive)
	{
		// OnProjectileStop clears the updated component, so restore it before every launch
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
	SetMovementActive(bInPlay);
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
		// net and other blockers only deflect the ball
		return;
	}

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

void AJGBall::UpdateShadow()
{
	const UWorld* World = GetWorld();
	const FVector Start = GetActorLocation();
	const FVector End = Start - FVector(0.0, 0.0, 5000.0);

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(JGBallShadow), false, this);
	const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_JGBall, Params);

	ShadowMesh->SetVisibility(bHit);
	if (!bHit)
	{
		return;
	}

	// the shadow shrinks with height to give a height cue
	const double Height = Start.Z - Hit.ImpactPoint.Z;
	const double Scale = FMath::GetMappedRangeValueClamped(TRange<double>(0.0, 400.0), TRange<double>(1.0, 0.5), Height);
	const double Diameter = VisualRadius * 2.0 * Scale;

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
