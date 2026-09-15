#include "Jokgu/Gameplay/JGCourt.h"
#include "Jokgu/Core/JGDebug.h"
#include "Jokgu/Core/JGMatchRules.h"
#include "Jokgu/Data/JGBalanceData.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "DrawDebugHelpers.h"

AJGCourt::AJGCourt()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	PlaceholderMaterial = BasicMaterial.Object;

	GroundCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("GroundCollision"));
	GroundCollision->SetupAttachment(Root);
	GroundCollision->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	GroundCollision->SetCollisionResponseToChannel(ECC_JGBall, ECR_Block);
	GroundCollision->ComponentTags.Add(JGTags::Ground());

	NetCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("NetCollision"));
	NetCollision->SetupAttachment(Root);
	NetCollision->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	NetCollision->SetCollisionResponseToChannel(ECC_JGBall, ECR_Block);
	NetCollision->ComponentTags.Add(JGTags::Net());

	NetPlayerBlocker = CreatePlayerBlocker(TEXT("NetPlayerBlocker"));
	SideBlockerLeft = CreatePlayerBlocker(TEXT("SideBlockerLeft"));
	SideBlockerRight = CreatePlayerBlocker(TEXT("SideBlockerRight"));
	BackBlockerA = CreatePlayerBlocker(TEXT("BackBlockerA"));
	BackBlockerB = CreatePlayerBlocker(TEXT("BackBlockerB"));

	GroundMesh = CreateVisualMesh(TEXT("GroundMesh"), CubeMesh.Object);
	CourtSurfaceMesh = CreateVisualMesh(TEXT("CourtSurfaceMesh"), CubeMesh.Object);
	LineSideLeft = CreateVisualMesh(TEXT("LineSideLeft"), CubeMesh.Object);
	LineSideRight = CreateVisualMesh(TEXT("LineSideRight"), CubeMesh.Object);
	LineBackA = CreateVisualMesh(TEXT("LineBackA"), CubeMesh.Object);
	LineBackB = CreateVisualMesh(TEXT("LineBackB"), CubeMesh.Object);
	LineCenter = CreateVisualMesh(TEXT("LineCenter"), CubeMesh.Object);
	NetMesh = CreateVisualMesh(TEXT("NetMesh"), CubeMesh.Object);
}

UBoxComponent* AJGCourt::CreatePlayerBlocker(FName Name)
{
	UBoxComponent* Box = CreateDefaultSubobject<UBoxComponent>(Name);
	Box->SetupAttachment(Root);
	Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Box->SetCollisionObjectType(ECC_WorldDynamic);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);
	Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Box->SetCanEverAffectNavigation(false);
	return Box;
}

UStaticMeshComponent* AJGCourt::CreateVisualMesh(FName Name, UStaticMesh* Mesh)
{
	UStaticMeshComponent* MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	MeshComponent->SetupAttachment(Root);
	MeshComponent->SetStaticMesh(Mesh);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetCanEverAffectNavigation(false);
	MeshComponent->SetGenerateOverlapEvents(false);
	return MeshComponent;
}

void AJGCourt::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	ApplyDimensions();
}

void AJGCourt::ApplyDimensions()
{
	const UJGBalanceData* Balance = UJGBalanceData::Get();

	const double HalfLength = GetHalfLength();
	const double HalfWidth = GetHalfWidth();
	const double Margin = Balance->PlayerAreaMargin;
	const double LineWidth = Balance->CourtLineWidth;
	const double NetHeight = Balance->NetHeight;
	const double BlockerHalfHeight = PlayerBlockerHeight * 0.5;
	const double GroundHalf = FMath::Max3<double>(GroundHalfExtent,HalfLength + Margin + 100.0, HalfWidth + Margin + 100.0);

	// collision: floor top at local Z = 0
	PlaceBox(GroundCollision, FVector(0.0, 0.0, -50.0), FVector(GroundHalf, GroundHalf, 50.0));
	PlaceBox(NetCollision, FVector(0.0, 0.0, NetHeight * 0.5), FVector(Balance->NetThickness * 0.5, HalfWidth + 50.0, NetHeight * 0.5));

	// player area: own half plus the defensive margin
	PlaceBox(NetPlayerBlocker, FVector(0.0, 0.0, BlockerHalfHeight), FVector(5.0, HalfWidth + Margin, BlockerHalfHeight));
	PlaceBox(SideBlockerLeft, FVector(0.0, -(HalfWidth + Margin + 5.0), BlockerHalfHeight), FVector(HalfLength + Margin, 5.0, BlockerHalfHeight));
	PlaceBox(SideBlockerRight, FVector(0.0, HalfWidth + Margin + 5.0, BlockerHalfHeight), FVector(HalfLength + Margin, 5.0, BlockerHalfHeight));
	PlaceBox(BackBlockerA, FVector(-(HalfLength + Margin + 5.0), 0.0, BlockerHalfHeight), FVector(5.0, HalfWidth + Margin, BlockerHalfHeight));
	PlaceBox(BackBlockerB, FVector(HalfLength + Margin + 5.0, 0.0, BlockerHalfHeight), FVector(5.0, HalfWidth + Margin, BlockerHalfHeight));

	// visuals: lines sit inside the court rectangle so their outer edge matches the judgement boundary
	PlaceCube(GroundMesh, FVector(0.0, 0.0, -1.0), FVector(GroundHalf * 2.0, GroundHalf * 2.0, 2.0));
	PlaceCube(CourtSurfaceMesh, FVector(0.0, 0.0, 0.25), FVector(HalfLength * 2.0, HalfWidth * 2.0, 0.5));
	PlaceCube(LineSideLeft, FVector(0.0, -(HalfWidth - LineWidth * 0.5), 0.5), FVector(HalfLength * 2.0, LineWidth, 1.0));
	PlaceCube(LineSideRight, FVector(0.0, HalfWidth - LineWidth * 0.5, 0.5), FVector(HalfLength * 2.0, LineWidth, 1.0));
	PlaceCube(LineBackA, FVector(-(HalfLength - LineWidth * 0.5), 0.0, 0.5), FVector(LineWidth, HalfWidth * 2.0, 1.0));
	PlaceCube(LineBackB, FVector(HalfLength - LineWidth * 0.5, 0.0, 0.5), FVector(LineWidth, HalfWidth * 2.0, 1.0));
	PlaceCube(LineCenter, FVector(0.0, 0.0, 0.5), FVector(LineWidth, HalfWidth * 2.0, 1.0));
	PlaceCube(NetMesh, FVector(0.0, 0.0, NetHeight * 0.5), FVector(Balance->NetThickness, HalfWidth * 2.0 + 100.0, NetHeight));

	ApplyColor(GroundMesh, GroundColor);
	ApplyColor(CourtSurfaceMesh, CourtColor);
	ApplyColor(LineSideLeft, LineColor);
	ApplyColor(LineSideRight, LineColor);
	ApplyColor(LineBackA, LineColor);
	ApplyColor(LineBackB, LineColor);
	ApplyColor(LineCenter, LineColor);
	ApplyColor(NetMesh, NetColor);
}

void AJGCourt::PlaceBox(UBoxComponent* Box, const FVector& LocalCenter, const FVector& HalfExtent) const
{
	Box->SetRelativeLocation(LocalCenter);
	Box->SetBoxExtent(HalfExtent, false);
}

void AJGCourt::PlaceCube(UStaticMeshComponent* Mesh, const FVector& LocalCenter, const FVector& Size) const
{
	// engine cube is 100 units wide with a centered pivot
	Mesh->SetRelativeLocation(LocalCenter);
	Mesh->SetRelativeScale3D(Size / 100.0);
}

void AJGCourt::ApplyColor(UStaticMeshComponent* Mesh, const FLinearColor& Color)
{
	if (!PlaceholderMaterial)
	{
		return;
	}

	UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));
	if (!Material || Material->Parent != PlaceholderMaterial)
	{
		Material = Mesh->CreateDynamicMaterialInstance(0, PlaceholderMaterial);
	}

	if (Material)
	{
		Material->SetVectorParameterValue(TEXT("Color"), Color);
	}
}

void AJGCourt::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (CVarJGDrawDebug.GetValueOnGameThread())
	{
		DrawDebugCourt();
	}
}

float AJGCourt::GetHalfLength() const
{
	return UJGBalanceData::Get()->CourtLength * 0.5f;
}

float AJGCourt::GetHalfWidth() const
{
	return UJGBalanceData::Get()->CourtWidth * 0.5f;
}

float AJGCourt::GetNetHeight() const
{
	return UJGBalanceData::Get()->NetHeight;
}

float AJGCourt::GetAttackSign(EJGTeam Team) const
{
	return Team == EJGTeam::B ? -1.0f : 1.0f;
}

FVector AJGCourt::GetAttackForward(EJGTeam Team) const
{
	return GetActorForwardVector() * GetAttackSign(Team);
}

EJGCourtZone AJGCourt::ClassifyWorldLocation(const FVector& WorldLocation) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(WorldLocation);
	return UJGMatchRules::ClassifyCourtPoint(FVector2D(Local.X, Local.Y), GetHalfLength(), GetHalfWidth());
}

FTransform AJGCourt::GetStartTransform(EJGTeam Team, bool bIsServing) const
{
	const UJGBalanceData* Balance = UJGBalanceData::Get();
	const double Distance = bIsServing ? Balance->ServeSpotDistanceFromNet : Balance->ReceiveSpotDistanceFromNet;
	const double Sign = GetAttackSign(Team);

	const FVector LocalLocation(-Sign * Distance, 0.0, Balance->PlayerSpawnHeight);
	const FRotator LocalRotation(0.0, Sign > 0.0 ? 0.0 : 180.0, 0.0);

	return FTransform(GetActorQuat() * LocalRotation.Quaternion(), GetActorTransform().TransformPosition(LocalLocation));
}

FVector AJGCourt::GetTeamCourtCenter(EJGTeam Team) const
{
	return GetActorTransform().TransformPosition(FVector(-GetAttackSign(Team) * GetHalfLength() * 0.5, 0.0, 0.0));
}

FTransform AJGCourt::GetCameraViewTransform(EJGTeam Team, const FVector& FollowWorldLocation) const
{
	const UJGBalanceData* Balance = UJGBalanceData::Get();
	const double Sign = GetAttackSign(Team);

	// lateral follow with a dead zone and a hard limit so the camera does not shake with every step
	const FVector FollowLocal = GetActorTransform().InverseTransformPosition(FollowWorldLocation);
	const double Excess = FMath::Max(0.0, FMath::Abs(FollowLocal.Y) - Balance->CameraLateralDeadZone);
	const double Lateral = FMath::Clamp(FMath::Sign(FollowLocal.Y) * Excess * Balance->CameraLateralFollowRatio,
		-Balance->CameraLateralMaxOffset, Balance->CameraLateralMaxOffset);

	const FVector FocusLocal(-Sign * Balance->CameraFocusDistanceFromNet, Lateral, 0.0);
	const FRotator LocalRotation(Balance->CameraPitch, Sign > 0.0 ? 0.0 : 180.0, 0.0);
	const FVector CameraLocal = FocusLocal - LocalRotation.Vector() * Balance->CameraDistance;

	return FTransform(GetActorQuat() * LocalRotation.Quaternion(), GetActorTransform().TransformPosition(CameraLocal));
}

void AJGCourt::DrawDebugCourt() const
{
#if ENABLE_DRAW_DEBUG
	const UWorld* World = GetWorld();
	const FTransform& Xf = GetActorTransform();
	const double HalfLength = GetHalfLength();
	const double HalfWidth = GetHalfWidth();
	const double Margin = UJGBalanceData::Get()->PlayerAreaMargin;

	auto DrawRect = [&](double MinX, double MaxX, double MinY, double MaxY, const FColor& Color)
	{
		const FVector P0 = Xf.TransformPosition(FVector(MinX, MinY, 3.0));
		const FVector P1 = Xf.TransformPosition(FVector(MaxX, MinY, 3.0));
		const FVector P2 = Xf.TransformPosition(FVector(MaxX, MaxY, 3.0));
		const FVector P3 = Xf.TransformPosition(FVector(MinX, MaxY, 3.0));
		DrawDebugLine(World, P0, P1, Color, false, -1.0f, 0, 2.0f);
		DrawDebugLine(World, P1, P2, Color, false, -1.0f, 0, 2.0f);
		DrawDebugLine(World, P2, P3, Color, false, -1.0f, 0, 2.0f);
		DrawDebugLine(World, P3, P0, Color, false, -1.0f, 0, 2.0f);
	};

	DrawRect(-HalfLength, 0.0, -HalfWidth, HalfWidth, FColor::Blue);
	DrawRect(0.0, HalfLength, -HalfWidth, HalfWidth, FColor::Red);
	DrawRect(-HalfLength - Margin, HalfLength + Margin, -HalfWidth - Margin, HalfWidth + Margin, FColor::Yellow);

	DrawDebugString(World, GetTeamCourtCenter(EJGTeam::A) + FVector(0, 0, 30), TEXT("Court A"), nullptr, FColor::Blue, 0.0f);
	DrawDebugString(World, GetTeamCourtCenter(EJGTeam::B) + FVector(0, 0, 30), TEXT("Court B"), nullptr, FColor::Red, 0.0f);
#endif
}
