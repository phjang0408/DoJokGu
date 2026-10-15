#include "Jokgu/Environment/JGTrafficLoop.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

AJGTrafficLoop::AJGTrafficLoop()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;

	Path = CreateDefaultSubobject<USplineComponent>(TEXT("Path"));
	Path->SetClosedLoop(true);
	RootComponent = Path;
}

void AJGTrafficLoop::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 에디터에서도 시작 위치의 차가 보이게 한다. 게임에서는 BeginPlay가 다시 만든다.
	const UWorld* World = GetWorld();
	if (World && !World->IsGameWorld())
	{
		RebuildCarComponents();
		UpdateCarTransforms();
	}
}

void AJGTrafficLoop::BeginPlay()
{
	Super::BeginPlay();

	TravelledDistance = 0.0f;
	RebuildCarComponents();
	UpdateCarTransforms();
}

void AJGTrafficLoop::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Length = Path->GetSplineLength();
	if (Length <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	TravelledDistance = FMath::Fmod(TravelledDistance + Speed * DeltaSeconds, Length);
	UpdateCarTransforms();
}

void AJGTrafficLoop::RebuildCarComponents()
{
	// CarComponents만 지우면 에디터에서 경로를 고칠 때마다 남은 차가 쌓인다(배열이 비워진 채로 다시 구성되는 경우).
	// 차 컴포넌트는 모두 Transient이므로 액터에 붙은 Transient 메시 컴포넌트를 전부 지운다.
	TInlineComponentArray<UStaticMeshComponent*> MeshComponents(this);
	for (UStaticMeshComponent* Component : MeshComponents)
	{
		if (Component && Component->HasAnyFlags(RF_Transient))
		{
			Component->DestroyComponent();
		}
	}
	CarComponents.Reset();

	for (const FJGTrafficCar& Car : Cars)
	{
		// 빈 칸도 자리를 맞추기 위해 null로 둔다
		UStaticMeshComponent* Component = nullptr;
		if (Car.Mesh)
		{
			Component = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
			Component->SetStaticMesh(Car.Mesh);
			Component->SetMobility(EComponentMobility::Movable);
			Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Component->SetCanEverAffectNavigation(false);
			Component->SetupAttachment(RootComponent);
			Component->RegisterComponent();
		}
		CarComponents.Add(Component);
	}
}

void AJGTrafficLoop::UpdateCarTransforms()
{
	const float Length = Path->GetSplineLength();
	if (Length <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	for (int32 Index = 0; Index < CarComponents.Num() && Index < Cars.Num(); ++Index)
	{
		UStaticMeshComponent* Component = CarComponents[Index];
		if (!Component)
		{
			continue;
		}

		const float Distance = FMath::Fmod(Cars[Index].StartDistance + TravelledDistance, Length);
		// 경로 높이는 도로 면이므로 회전은 수평(Yaw)만 쓴다
		FTransform OnPath = Path->GetTransformAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
		OnPath.SetRotation(FRotator(0.0, OnPath.Rotator().Yaw, 0.0).Quaternion());
		OnPath.SetScale3D(FVector::OneVector);
		Component->SetWorldTransform(Cars[Index].MeshOffset * OnPath);
	}
}
