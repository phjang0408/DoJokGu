#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Jokgu/Core/JGTypes.h"
#include "JGBallLauncher.generated.h"

class UArrowComponent;
class UStaticMeshComponent;

/** 발사기 한 발의 설정: 이름과 플레이어 타격과 같은 공격 입력 */
USTRUCT(BlueprintType)
struct FJGLaunchPreset
{
	GENERATED_BODY()

	/** 로그에 찍히는 이름 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Launch")
	FName Label = TEXT("Medium");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Launch")
	FJGAttackInput Input;
};

/**
 *  혼자 테스트하는 장치: 플레이어 타격과 같은 샷 계산으로 경기 공을 반복해서 쏜다.
 *  게임 모드가 연습 모드일 때만 동작한다. AI 상대의 기반으로 쓰지 않는다.
 *  친 쪽 진영에 놓고, ReceiverTeam에는 공을 받을 쪽을 지정한다.
 */
UCLASS()
class AJGBallLauncher : public AActor
{
	GENERATED_BODY()

#pragma region 컴포넌트

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> Root;

	/** 레벨에서 위치를 알아보기 위한 원뿔. 충돌 없음. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> BodyMesh;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UArrowComponent> Arrow;
#endif

#pragma endregion

protected:

#pragma region 설정

	/** 공을 받을 팀. 발사는 그 상대 팀이 친 것으로 기록한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Launcher")
	EJGTeam ReceiverTeam = EJGTeam::A;

	/** 발사기 원점 위 공의 발사 높이 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Launcher", meta=(Units="cm"))
	float LaunchHeight = 60.0f;

	/** 켜면 BeginPlay부터 LaunchInterval마다 자동으로 쏜다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Launcher")
	bool bAutoLaunch = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Launcher", meta=(ClampMin=0.2, Units="s"))
	float LaunchInterval = 3.0f;

	/** 프리셋을 순서대로 쏘고 반복한다. 기본값: 약/중/강/최대 드래그와 탭. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Launcher")
	TArray<FJGLaunchPreset> Presets;

#pragma endregion

public:

	AJGBallLauncher();

	/** 다음 프리셋을 쏜다 (서버 전용) */
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Launcher")
	void LaunchNext();

protected:

	virtual void BeginPlay() override;

	FTimerHandle LaunchTimer;
	int32 NextPresetIndex = 0;
};
