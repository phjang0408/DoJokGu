#include "Jokgu/Lobby/JGInteractZone.h"
#include "Jokgu/Lobby/JGLobbyPlayerController.h"
#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/Pawn.h"

AJGInteractZone::AJGInteractZone()
{
	PrimaryActorTick.bCanEverTick = false;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->InitBoxExtent(FVector(250.0, 250.0, 120.0));
	// Pawn만 감지하고 아무것도 막지 않는다
	Box->SetCollisionProfileName(UCollisionProfile::CustomCollisionProfileName);
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);
	Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Box->SetGenerateOverlapEvents(true);
	RootComponent = Box;

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Box);
	Label->SetRelativeLocation(FVector(0.0, 0.0, 260.0));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(70.0f);
	Label->SetTextRenderColor(FColor(255, 200, 60));
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

FText AJGInteractZone::GetDisplayName() const
{
	return Kind == EJGZoneKind::Match ? FText::FromString(TEXT("Play")) : FText::FromString(TEXT("Settings"));
}

void AJGInteractZone::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	Label->SetText(GetDisplayName());
}

void AJGInteractZone::BeginPlay()
{
	Super::BeginPlay();

	Box->OnComponentBeginOverlap.AddUniqueDynamic(this, &AJGInteractZone::HandleBeginOverlap);
	Box->OnComponentEndOverlap.AddUniqueDynamic(this, &AJGInteractZone::HandleEndOverlap);
}

void AJGInteractZone::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	NotifyLocalPawn(OtherActor, true);
}

void AJGInteractZone::HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	NotifyLocalPawn(OtherActor, false);
}

void AJGInteractZone::NotifyLocalPawn(AActor* OtherActor, bool bEntered)
{
	// AI 학생 등 다른 Pawn은 무시한다
	const APawn* Pawn = Cast<APawn>(OtherActor);
	AJGLobbyPlayerController* PlayerController = Pawn && Pawn->IsLocallyControlled() ? Pawn->GetController<AJGLobbyPlayerController>() : nullptr;

	if (PlayerController)
	{
		PlayerController->SetZoneOverlap(this, bEntered);
	}
}
