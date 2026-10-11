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

#pragma region 생성

AJGCourt::AJGCourt()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	PlaceholderMaterial = BasicMaterial.Object;

	// 바닥과 네트는 모든 것을 막고, 공 채널(기본 Ignore)도 명시적으로 막아야 공이 튕긴다
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
	// Pawn만 막고 나머지(공 포함)는 모두 통과시킨다
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

#pragma endregion

#pragma region 크기 적용

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
	// 바닥은 플레이어 영역보다 최소 1m 더 넓게 깐다
	const double GroundHalf = FMath::Max3<double>(GroundHalfExtent,HalfLength + Margin + 100.0, HalfWidth + Margin + 100.0);

	// 충돌: 바닥 윗면이 로컬 Z = 0
	PlaceBox(GroundCollision, FVector(0.0, 0.0, -50.0), FVector(GroundHalf, GroundHalf, 50.0));
	PlaceBox(NetCollision, FVector(0.0, 0.0, NetHeight * 0.5), FVector(Balance->NetThickness * 0.5, HalfWidth + 50.0, NetHeight * 0.5));

	// 플레이어 영역: 자기 진영 절반 + 수비 여유 공간. 벽 두께는 10cm.
	PlaceBox(NetPlayerBlocker, FVector(0.0, 0.0, BlockerHalfHeight), FVector(5.0, HalfWidth + Margin, BlockerHalfHeight));
	PlaceBox(SideBlockerLeft, FVector(0.0, -(HalfWidth + Margin + 5.0), BlockerHalfHeight), FVector(HalfLength + Margin, 5.0, BlockerHalfHeight));
	PlaceBox(SideBlockerRight, FVector(0.0, HalfWidth + Margin + 5.0, BlockerHalfHeight), FVector(HalfLength + Margin, 5.0, BlockerHalfHeight));
	PlaceBox(BackBlockerA, FVector(-(HalfLength + Margin + 5.0), 0.0, BlockerHalfHeight), FVector(5.0, HalfWidth + Margin, BlockerHalfHeight));
	PlaceBox(BackBlockerB, FVector(HalfLength + Margin + 5.0, 0.0, BlockerHalfHeight), FVector(5.0, HalfWidth + Margin, BlockerHalfHeight));

	// 표시: 선을 코트 사각형 안쪽에 놓아 선의 바깥 가장자리가 판정 경계와 맞게 한다
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

	for (UStaticMeshComponent* Mesh : { GroundMesh, CourtSurfaceMesh, LineSideLeft, LineSideRight, LineBackA, LineBackB, LineCenter, NetMesh })
	{
		Mesh->SetVisibility(bShowPlaceholderVisuals);
	}
}

void AJGCourt::PlaceBox(UBoxComponent* Box, const FVector& LocalCenter, const FVector& HalfExtent) const
{
	Box->SetRelativeLocation(LocalCenter);
	Box->SetBoxExtent(HalfExtent, false);
}

void AJGCourt::PlaceCube(UStaticMeshComponent* Mesh, const FVector& LocalCenter, const FVector& Size) const
{
	// 엔진 큐브는 한 변이 100 단위이고 피벗이 중심에 있다
	Mesh->SetRelativeLocation(LocalCenter);
	Mesh->SetRelativeScale3D(Size / 100.0);
}

void AJGCourt::ApplyColor(UStaticMeshComponent* Mesh, const FLinearColor& Color)
{
	if (!PlaceholderMaterial)
	{
		return;
	}

	// OnConstruction이 반복 호출돼도 동적 재질을 매번 새로 만들지 않고 재사용한다
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

#pragma endregion

void AJGCourt::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (CVarJGDrawDebug.GetValueOnGameThread())
	{
		DrawDebugCourt();
	}
}

#pragma region 판정 좌표 조회

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

#pragma endregion

#pragma region 시작 위치와 카메라

FTransform AJGCourt::GetStartTransform(EJGTeam Team, bool bIsServing) const
{
	const UJGBalanceData* Balance = UJGBalanceData::Get();
	const double Distance = bIsServing ? Balance->ServeSpotDistanceFromNet : Balance->ReceiveSpotDistanceFromNet;
	const double Sign = GetAttackSign(Team);

	// 자기 진영(공격 방향의 반대쪽)에 서서 공격 방향을 바라본다
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

	// 데드존과 최대치를 둔 좌우 추적. 한 걸음마다 카메라가 흔들리지 않게 한다.
	const FVector FollowLocal = GetActorTransform().InverseTransformPosition(FollowWorldLocation);
	const double Excess = FMath::Max(0.0, FMath::Abs(FollowLocal.Y) - Balance->CameraLateralDeadZone);
	const double Lateral = FMath::Clamp(FMath::Sign(FollowLocal.Y) * Excess * Balance->CameraLateralFollowRatio,
		-Balance->CameraLateralMaxOffset, Balance->CameraLateralMaxOffset);

	// 초점에서 시선 반대 방향으로 CameraDistance만큼 물러난 곳이 카메라 위치다
	const FVector FocusLocal(-Sign * Balance->CameraFocusDistanceFromNet, Lateral, 0.0);
	const FRotator LocalRotation(Balance->CameraPitch, Sign > 0.0 ? 0.0 : 180.0, 0.0);
	const FVector CameraLocal = FocusLocal - LocalRotation.Vector() * Balance->CameraDistance;

	return FTransform(GetActorQuat() * LocalRotation.Quaternion(), GetActorTransform().TransformPosition(CameraLocal));
}

#pragma endregion

#pragma region 디버그 표시

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

	// 파랑 = A 코트, 빨강 = B 코트, 노랑 = 플레이어 이동 영역
	DrawRect(-HalfLength, 0.0, -HalfWidth, HalfWidth, FColor::Blue);
	DrawRect(0.0, HalfLength, -HalfWidth, HalfWidth, FColor::Red);
	DrawRect(-HalfLength - Margin, HalfLength + Margin, -HalfWidth - Margin, HalfWidth + Margin, FColor::Yellow);

	DrawDebugString(World, GetTeamCourtCenter(EJGTeam::A) + FVector(0, 0, 30), TEXT("Court A"), nullptr, FColor::Blue, 0.0f);
	DrawDebugString(World, GetTeamCourtCenter(EJGTeam::B) + FVector(0, 0, 30), TEXT("Court B"), nullptr, FColor::Red, 0.0f);
#endif
}

#pragma endregion
