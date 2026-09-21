#include "Jokgu/Player/JGAnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "Animation/AnimSequence.h"
#include "AnimNodes/AnimNode_Slot.h"
#include "AnimNodes/AnimNode_TwoWayBlend.h"
#include "GameFramework/Actor.h"

struct FJGAnimInstanceProxy : FAnimInstanceProxy
{
	FAnimNode_SequencePlayer_Standalone Idle;
	FAnimNode_SequencePlayer_Standalone Walk;
	FAnimNode_SequencePlayer_Standalone Run;
	FAnimNode_TwoWayBlend IdleWalk;
	FAnimNode_TwoWayBlend Locomotion;
	FAnimNode_Slot Slot;
	float Speed = 0.f;
	float WalkSpeed = 180.f;
	float RunSpeed = 450.f;

	explicit FJGAnimInstanceProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance)
	{
		IdleWalk.A.SetLinkNode(&Idle);
		IdleWalk.B.SetLinkNode(&Walk);
		Locomotion.A.SetLinkNode(&IdleWalk);
		Locomotion.B.SetLinkNode(&Run);
		Slot.Source.SetLinkNode(&Locomotion);
		Slot.SlotName = TEXT("DefaultSlot");
		Slot.bAlwaysUpdateSourcePose = true;
	}

	virtual FAnimNode_Base* GetCustomRootNode() override { return &Slot; }
	virtual void GetCustomNodes(TArray<FAnimNode_Base*>& OutNodes) override
	{
		OutNodes.Append({&Idle, &Walk, &Run, &IdleWalk, &Locomotion, &Slot});
	}

	virtual void Initialize(UAnimInstance* Instance) override
	{
		UJGAnimInstance* JG = CastChecked<UJGAnimInstance>(Instance);
		JG->LoadedIdle = JG->IdleAnimation.LoadSynchronous();
		JG->LoadedWalk = JG->WalkAnimation.LoadSynchronous();
		JG->LoadedRun = JG->RunAnimation.LoadSynchronous();
		Idle.SetSequence(JG->LoadedIdle);
		Walk.SetSequence(JG->LoadedWalk ? JG->LoadedWalk : JG->LoadedIdle);
		Run.SetSequence(JG->LoadedRun ? JG->LoadedRun : JG->LoadedWalk);
		Idle.SetLoopAnimation(true);
		Walk.SetLoopAnimation(true);
		Run.SetLoopAnimation(true);
		Walk.SetGroupName(TEXT("Locomotion"));
		Run.SetGroupName(TEXT("Locomotion"));
		Walk.SetGroupMethod(EAnimSyncMethod::SyncGroup);
		Run.SetGroupMethod(EAnimSyncMethod::SyncGroup);
		WalkSpeed = FMath::Max(1.f, JG->WalkSpeed);
		RunSpeed = FMath::Max(WalkSpeed + 1.f, JG->RunSpeed);
		FAnimInstanceProxy::Initialize(Instance);
	}

	virtual void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override
	{
		FAnimInstanceProxy::PreUpdate(Instance, DeltaSeconds);
		const AActor* Owner = Instance->GetOwningActor();
		Speed = Owner ? Owner->GetVelocity().Size2D() : 0.f;
	}

	virtual void Update(float DeltaSeconds) override
	{
		IdleWalk.Alpha = FMath::Clamp(Speed / WalkSpeed, 0.f, 1.f);
		Locomotion.Alpha = FMath::Clamp((Speed - WalkSpeed) / (RunSpeed - WalkSpeed), 0.f, 1.f);
		Walk.SetPlayRate(FMath::Clamp(Speed / 80.f, 0.65f, 3.5f));
		Run.SetPlayRate(FMath::Clamp(Speed / 180.f, 0.65f, 3.5f));
	}
};

UJGAnimInstance::UJGAnimInstance()
{
	IdleAnimation = FSoftObjectPath(TEXT("/Game/Jokgu/Characters/Rigged/Animations/A_DJG_Idle.A_DJG_Idle"));
	WalkAnimation = FSoftObjectPath(TEXT("/Game/Jokgu/Characters/Rigged/Animations/A_DJG_Walk.A_DJG_Walk"));
	RunAnimation = FSoftObjectPath(TEXT("/Game/Jokgu/Characters/Rigged/Animations/A_DJG_Run.A_DJG_Run"));
	RootMotionMode = ERootMotionMode::IgnoreRootMotion;
}

FAnimInstanceProxy* UJGAnimInstance::CreateAnimInstanceProxy() { return new FJGAnimInstanceProxy(this); }
void UJGAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) { delete InProxy; }
