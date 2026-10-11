#include "Jokgu/Player/JGAnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "Animation/AnimSequence.h"
#include "AnimNodes/AnimNode_Slot.h"
#include "AnimNodes/AnimNode_TwoWayBlend.h"
#include "GameFramework/Actor.h"

/**
 *  애니메이션 그래프를 코드로 구성한 프록시.
 *  Idle --IdleWalk--> Walk, 그 결과 --Locomotion--> Run, 마지막에 DefaultSlot(몽타주)이 위에 덮인다.
 */
struct FJGAnimInstanceProxy : FAnimInstanceProxy
{
	FAnimNode_SequencePlayer_Standalone Idle;
	FAnimNode_SequencePlayer_Standalone Walk;
	FAnimNode_SequencePlayer_Standalone Run;
	FAnimNode_TwoWayBlend IdleWalk;
	FAnimNode_TwoWayBlend Locomotion;
	FAnimNode_Slot Slot;

	/** 게임 스레드에서 읽어 둔 수평 속도 */
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
		// 몽타주 재생 중에도 아래 이동 블렌드를 계속 갱신해서 끝날 때 자연스럽게 돌아온다
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

		// 걷기·달리기가 없으면 한 단계 아래 애니메이션으로 대신한다
		Idle.SetSequence(JG->LoadedIdle);
		Walk.SetSequence(JG->LoadedWalk ? JG->LoadedWalk : JG->LoadedIdle);
		Run.SetSequence(JG->LoadedRun ? JG->LoadedRun : JG->LoadedWalk);
		Idle.SetLoopAnimation(true);
		Walk.SetLoopAnimation(true);
		Run.SetLoopAnimation(true);

		// 걷기와 달리기는 싱크 그룹으로 발 위치를 맞춘다
		Walk.SetGroupName(TEXT("Locomotion"));
		Run.SetGroupName(TEXT("Locomotion"));
		Walk.SetGroupMethod(EAnimSyncMethod::SyncGroup);
		Run.SetGroupMethod(EAnimSyncMethod::SyncGroup);

		// 0으로 나누지 않도록 RunSpeed는 WalkSpeed보다 항상 크게 둔다
		WalkSpeed = FMath::Max(1.f, JG->WalkSpeed);
		RunSpeed = FMath::Max(WalkSpeed + 1.f, JG->RunSpeed);
		FAnimInstanceProxy::Initialize(Instance);
	}

	virtual void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override
	{
		// 게임 스레드: 액터 속도를 읽어 워커 스레드의 Update로 넘긴다
		FAnimInstanceProxy::PreUpdate(Instance, DeltaSeconds);
		const AActor* Owner = Instance->GetOwningActor();
		Speed = Owner ? Owner->GetVelocity().Size2D() : 0.f;
	}

	virtual void Update(float DeltaSeconds) override
	{
		// 0 ~ WalkSpeed에서 Idle->Walk, WalkSpeed ~ RunSpeed에서 Walk->Run으로 섞는다
		IdleWalk.Alpha = FMath::Clamp(Speed / WalkSpeed, 0.f, 1.f);
		Locomotion.Alpha = FMath::Clamp((Speed - WalkSpeed) / (RunSpeed - WalkSpeed), 0.f, 1.f);
		// 재생 속도를 이동 속도에 맞춰 발 미끄러짐을 줄인다
		Walk.SetPlayRate(FMath::Clamp(Speed / 80.f, 0.65f, 3.5f));
		Run.SetPlayRate(FMath::Clamp(Speed / 180.f, 0.65f, 3.5f));
	}
};

UJGAnimInstance::UJGAnimInstance()
{
	IdleAnimation = FSoftObjectPath(TEXT("/Game/Jokgu/Characters/Rigged/Animations/A_DJG_Idle.A_DJG_Idle"));
	WalkAnimation = FSoftObjectPath(TEXT("/Game/Jokgu/Characters/Rigged/Animations/A_DJG_Walk.A_DJG_Walk"));
	RunAnimation = FSoftObjectPath(TEXT("/Game/Jokgu/Characters/Rigged/Animations/A_DJG_Run.A_DJG_Run"));
	// 이동은 CharacterMovement가 하므로 애니메이션의 루트 모션은 쓰지 않는다
	RootMotionMode = ERootMotionMode::IgnoreRootMotion;
}

FAnimInstanceProxy* UJGAnimInstance::CreateAnimInstanceProxy() { return new FJGAnimInstanceProxy(this); }
void UJGAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) { delete InProxy; }
