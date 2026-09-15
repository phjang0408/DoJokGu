#include "Jokgu/Gameplay/JGBallLauncher.h"
#include "Jokgu/Core/JGGameModeBase.h"
#include "Jokgu/Core/JGGameStateBase.h"
#include "Jokgu/Core/JGMatchRules.h"
#include "Jokgu/Gameplay/JGBall.h"
#include "Jokgu/Gameplay/JGCourt.h"
#include "Jokgu/Gameplay/JGShotCalculator.h"
#include "Components/ArrowComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"
#include "DoJokgu.h"

AJGBallLauncher::AJGBallLauncher()
{
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(Root);
	BodyMesh->SetStaticMesh(ConeMesh.Object);
	BodyMesh->SetRelativeScale3D(FVector(0.4f));
	BodyMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 20.0f));
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

#if WITH_EDITORONLY_DATA
	Arrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	if (Arrow)
	{
		Arrow->SetupAttachment(Root);
	}
#endif

	auto AddPreset = [this](FName Label, EJGAttackType Type, float Power, float Angle)
	{
		FJGLaunchPreset& Preset = Presets.AddDefaulted_GetRef();
		Preset.Label = Label;
		Preset.Input.Type = Type;
		Preset.Input.Power = Power;
		Preset.Input.AimAngleDeg = Angle;
	};

	AddPreset(TEXT("Weak"), EJGAttackType::Drag, 0.2f, 0.0f);
	AddPreset(TEXT("Medium"), EJGAttackType::Drag, 0.5f, 0.0f);
	AddPreset(TEXT("Strong"), EJGAttackType::Drag, 0.8f, 0.0f);
	AddPreset(TEXT("Full"), EJGAttackType::Drag, 1.0f, 0.0f);
	AddPreset(TEXT("Tap"), EJGAttackType::Tap, 0.0f, 0.0f);
}

void AJGBallLauncher::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && bAutoLaunch)
	{
		GetWorldTimerManager().SetTimer(LaunchTimer, this, &AJGBallLauncher::LaunchNext, LaunchInterval, true, LaunchInterval);
	}
}

void AJGBallLauncher::LaunchNext()
{
	if (!HasAuthority() || Presets.IsEmpty())
	{
		return;
	}

	const AJGGameModeBase* GameMode = GetWorld()->GetAuthGameMode<AJGGameModeBase>();
	const AJGGameStateBase* GameState = GetWorld()->GetGameState<AJGGameStateBase>();

	if (!GameMode || !GameMode->IsPracticeMode())
	{
		UE_LOG(LogDoJokgu, Warning, TEXT("%s only fires in practice mode."), *GetName());
		GetWorldTimerManager().ClearTimer(LaunchTimer);
		return;
	}

	AJGBall* Ball = GameState ? GameState->GetBall() : nullptr;
	AJGCourt* Court = GameState ? GameState->GetCourt() : nullptr;
	if (!Ball || !Court)
	{
		return;
	}

	const FJGLaunchPreset& Preset = Presets[NextPresetIndex];
	NextPresetIndex = (NextPresetIndex + 1) % Presets.Num();

	const EJGTeam HitterTeam = UJGMatchRules::GetOpposingTeam(ReceiverTeam);
	const FVector Start = GetActorLocation() + FVector(0.0, 0.0, LaunchHeight);

	Ball->HoldAt(Start);
	const FVector Velocity = UJGShotCalculator::ComputeAttackVelocity(*Court, HitterTeam, Start, Preset.Input, 1.0f, 0.0f, Ball->GetBallGravityZ());
	Ball->LaunchByHit(Velocity, HitterTeam);

	UE_LOG(LogDoJokgu, Log, TEXT("Launcher fired '%s' (power %.2f, angle %.1f) velocity %s"),
		*Preset.Label.ToString(), Preset.Input.Power, Preset.Input.AimAngleDeg, *Velocity.ToCompactString());
}
