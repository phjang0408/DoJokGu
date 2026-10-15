#include "Jokgu/Lobby/JGLobbyCharacter.h"
#include "Jokgu/Data/JGCharacterData.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"

AJGLobbyCharacter::AJGLobbyCharacter()
{
	// 경기 캐릭터와 같은 캡슐·메시 위치를 써서 같은 선수 메시가 바닥에 맞게 선다
	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);
	GetMesh()->SetRelativeLocation(FVector(0.0, 0.0, -96.0));

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// 스틱을 끝까지 밀면 달리기, 조금 밀면 걷기(애님 블렌드가 속도로 고른다)
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
	Movement->MaxWalkSpeed = 500.0f;
	Movement->MinAnalogWalkSpeed = 40.0f;
	Movement->BrakingDecelerationWalking = 2000.0f;
	// NavAgentProps.bCanJump는 IsJumpAllowed()도 읽는다. 끄면 Jump()가 무시된다.
	Movement->GetNavAgentPropertiesRef().bCanJump = true;
	// 점프 중에도 방향키 입력이 땅에서와 같은 가속도로 들어간다(엔진 기본 0.05는 거의 무시됨)
	Movement->AirControl = 1.0f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 520.0f;
	CameraBoom->SocketOffset = FVector(0.0, 0.0, 120.0);
	CameraBoom->bUsePawnControlRotation = true;
	// 학교 배경은 경기 카메라를 위해 Camera 채널을 무시하므로, 붐은 Visibility로 벽을 감지한다
	CameraBoom->ProbeChannel = ECC_Visibility;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 10.0f;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	// 붐 끝에서 캐릭터를 약간 내려다본다
	FollowCamera->SetRelativeRotation(FRotator(-12.0, 0.0, 0.0));
}

void AJGLobbyCharacter::ApplyLook(const UJGCharacterData* Data)
{
	if (!Data)
	{
		return;
	}

	if (Data->SkeletalMesh)
	{
		GetMesh()->SetSkeletalMeshAsset(Data->SkeletalMesh);
	}

	if (Data->AnimClass)
	{
		GetMesh()->SetAnimInstanceClass(Data->AnimClass);
	}
}

void AJGLobbyCharacter::DoStickMove(float Right, float Forward)
{
	if (!Controller)
	{
		return;
	}

	// 카메라가 보는 방향을 바닥에 투영한 축 기준으로 움직인다
	const FRotator YawRotation(0.0, Controller->GetControlRotation().Yaw, 0.0);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Forward);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Right);
}

void AJGLobbyCharacter::AddCameraYaw(float Degrees)
{
	AddControllerYawInput(Degrees);
}

void AJGLobbyCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInputComponent)
	{
		return;
	}

	if (MoveAction)
	{
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AJGLobbyCharacter::Move);
	}

	if (JumpAction)
	{
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AJGLobbyCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AJGLobbyCharacter::DoJumpEnd);
	}
}

void AJGLobbyCharacter::DoJumpStart()
{
	Jump();
}

void AJGLobbyCharacter::DoJumpEnd()
{
	StopJumping();
}

void AJGLobbyCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	DoStickMove(Axis.X, Axis.Y);
}
