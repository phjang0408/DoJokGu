#include "Jokgu/Lobby/JGStudentCharacter.h"
#include "Jokgu/Data/JGCharacterData.h"
#include "Jokgu/Lobby/JGStudentAIController.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

AJGStudentCharacter::AJGStudentCharacter()
{
	bReplicates = false;
	SetReplicateMovement(false);

	// 선수와 같은 캡슐·메시 위치
	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);
	GetMesh()->SetRelativeLocation(FVector(0.0, 0.0, -96.0));
	// 로비 카메라 붐은 Visibility로 벽을 감지하므로, 학생이 카메라를 당기지 않게 한다
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	AIControllerClass = AJGStudentAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationYaw = false;

	// 걷기 애니메이션이 100%가 되는 속도(180) 근처로 천천히 걷는다
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.0f, 300.0f, 0.0f);
	Movement->MaxWalkSpeed = 150.0f;
	Movement->BrakingDecelerationWalking = 600.0f;
	Movement->bUseRVOAvoidance = true;
	Movement->AvoidanceConsiderationRadius = 300.0f;
	Movement->GetNavAgentPropertiesRef().bCanJump = false;
}

void AJGStudentCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 에디터에서 배치했을 때도 외형이 보이게 한다
	ApplyLook();
}

void AJGStudentCharacter::BeginPlay()
{
	Super::BeginPlay();

	ApplyLook();
	HomeLocation = GetActorLocation();
}

void AJGStudentCharacter::ApplyLook()
{
	if (!LookData)
	{
		return;
	}

	if (LookData->SkeletalMesh)
	{
		GetMesh()->SetSkeletalMeshAsset(LookData->SkeletalMesh);
	}

	if (LookData->AnimClass)
	{
		GetMesh()->SetAnimInstanceClass(LookData->AnimClass);
	}
}

float AJGStudentCharacter::PlayRandomEmote()
{
	TArray<UAnimMontage*> Valid;
	for (UAnimMontage* Montage : EmoteMontages)
	{
		if (Montage)
		{
			Valid.Add(Montage);
		}
	}

	if (Valid.IsEmpty())
	{
		return 0.0f;
	}

	UAnimMontage* Montage = Valid[FMath::RandHelper(Valid.Num())];
	const float Length = PlayAnimMontage(Montage);
	return Length;
}
