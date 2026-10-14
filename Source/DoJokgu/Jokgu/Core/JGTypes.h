#pragma once

#include "CoreMinimal.h"
#include "JGTypes.generated.h"

#pragma region 충돌 채널과 태그

/** 공 전용 오브젝트 채널. DefaultEngine.ini에 "JGBall"로 등록되어 있고 기본 반응이 Ignore라서
 *  명시적으로 막는 면(코트 바닥, 네트)만 공을 튕길 수 있다. */
#define ECC_JGBall ECC_GameTraceChannel1

namespace JGTags
{
	/** 랠리 규칙에서 지면 접촉으로 치는 면에 붙이는 컴포넌트 태그 */
	inline FName Ground() { static const FName Tag(TEXT("JGGround")); return Tag; }

	/** 네트 충돌체에 붙이는 컴포넌트 태그 */
	inline FName Net() { static const FName Tag(TEXT("JGNet")); return Tag; }
}

#pragma endregion

#pragma region 열거형

/** 팀이자 코트 진영. A팀은 코트의 -X 절반을 쓰고 +X 방향으로 공격한다. */
UENUM(BlueprintType)
enum class EJGTeam : uint8
{
	None,
	A,
	B
};

/** 경기 진행 단계. 단계마다 받는 입력과 끝나는 조건이 다르다. */
UENUM(BlueprintType)
enum class EJGMatchPhase : uint8
{
	/** 두 번째 플레이어를 기다리는 중 */
	WaitingPlayers,
	/** 득점 후 위치를 정리하고 서브를 준비하는 짧은 대기 */
	PreparingServe,
	/** 서브하는 쪽이 공을 들고 있고 제한 시간이 흐르는 중 */
	Serving,
	/** 공이 살아 있는 랠리 */
	Rally,
	/** 득점이 결정된 뒤 다음 서브까지의 대기 */
	PointEnd,
	/** 경기 종료, 재경기 투표를 받는다 */
	MatchEnd
};

/** 지면 접촉이 일어난 위치를 코트 기준으로 나눈 것 */
UENUM(BlueprintType)
enum class EJGCourtZone : uint8
{
	Out,
	CourtA,
	CourtB
};

/** 터치를 뗄 때 확정되는 공격 동작 */
UENUM(BlueprintType)
enum class EJGAttackType : uint8
{
	None,
	Tap,
	Drag
};

/** 킥에 재생할 애니메이션 슬롯. 동작만 고르며, 공 속도는 언제나 연속적인 파워 값에서 나온다. */
UENUM(BlueprintType)
enum class EJGKickMotion : uint8
{
	Tap,
	Weak,
	Medium,
	Strong,
	Serve,
	Slide
};

/** 슬라이딩 상태. Ready -> Sliding -> Recovery -> Ready 순서로 돈다. */
UENUM(BlueprintType)
enum class EJGSlideState : uint8
{
	Ready,
	Sliding,
	Recovery
};

/** 득점 사유 */
UENUM(BlueprintType)
enum class EJGPointReason : uint8
{
	None,
	/** 첫 착지가 코트 밖: 친 쪽이 실점한다 */
	OutOnFirstLanding,
	/** 착지하거나 멈추기 전에 받는 쪽 코트에 닿지 못함(네트, 자기 코트): 친 쪽이 실점한다 */
	FailedToCross,
	/** 받는 쪽 코트에 착지한 뒤 다시 지면에 닿음: 받는 쪽이 실점한다 */
	NotReturned,
	/** 서브하는 쪽이 제한 시간 안에 서브하지 않음 */
	ServeTimeout
};

/** 경기 종료 사유 */
UENUM(BlueprintType)
enum class EJGMatchEndReason : uint8
{
	None,
	ScoreReached,
	PlayerLeft
};

#pragma endregion

#pragma region 구조체

/** 소유 클라이언트가 서버로 보내는 공격 요청 */
USTRUCT(BlueprintType)
struct FJGAttackInput
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
	EJGAttackType Type = EJGAttackType::None;

	/** 내 코트에서 상대 코트를 향하는 방향 기준의 수평 조준 각도(도). 양수가 플레이어의 오른쪽이다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
	float AimAngleDeg = 0.0f;

	/** [0, 1]로 정규화한 드래그 파워. 탭에서는 쓰지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
	float Power = 0.0f;

	/** 클라이언트가 매기는 순번. 로그 확인용이다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
	int32 InputId = 0;

	/** 진단용: 요청한 순간 클라이언트 화면의 공(보이는 메시)과 발 사이 수평 거리. 음수면 공을 못 찾았다.
	 *  판정에는 쓰지 않고, 서버가 거절했을 때 "화면에서는 닿았는지" 로그에 남기는 데만 쓴다. */
	UPROPERTY()
	float ClientBallDistance = -1.0f;

	/** 진단용: 같은 순간 발 기준 공 중심 높이 */
	UPROPERTY()
	float ClientBallHeight = 0.0f;

	bool IsValid() const { return Type != EJGAttackType::None; }
};

/** 서버가 지면 접촉을 판정하는 데 필요한 모든 것. 공이 가지고 있다. */
USTRUCT(BlueprintType)
struct FJGRallyState
{
	GENERATED_BODY()

	/** 마지막으로 공을 친 팀. None이면 랠리 중이 아니다(서브 대기, 득점 결정 후). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rally")
	EJGTeam LastHitterTeam = EJGTeam::None;

	/** 다음에 공을 받아야 하는 팀. 이 팀만 공을 칠 수 있다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rally")
	EJGTeam ExpectedReceiverTeam = EJGTeam::None;

	/** 받는 쪽 코트에서 공이 튄 횟수 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rally")
	int32 ReceiverBounceCount = 0;

	/** 이번 샷이 받는 쪽 코트에 한 번 정상 착지했는지 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rally")
	bool bHasLandedInReceiverCourt = false;

	/** 합법 타격마다 1씩 늘어난다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rally")
	int32 ShotId = 0;

	/** 공 중심이 받는 쪽 코트 방향으로 네트 평면을 넘을 때 샷마다 한 번 기록한다. 진단용이다:
	 *  받는 쪽은 공이 넘어오기 전에 쳐도 합법이므로 득점은 여전히 착지 구역으로 정한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rally")
	bool bCrossedNet = false;

	/** 넘는 순간 공 아래쪽과 네트 위 사이의 높이 차이. 음수면 공이 네트에 닿은 것이다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rally")
	float NetCrossingClearance = 0.0f;

	/** 넘는 순간 공의 코트 로컬 Y. 코트 반폭을 넘으면 공이 코트 옆으로 지나간 것이다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rally")
	float NetCrossingLateral = 0.0f;
};

/** 지면 접촉이나 멈춘 공을 판정한 결과 */
USTRUCT(BlueprintType)
struct FJGContactResult
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rally")
	bool bPointScored = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rally")
	EJGTeam ScoringTeam = EJGTeam::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rally")
	EJGPointReason Reason = EJGPointReason::None;
};

/** 모든 클라이언트가 똑같이 알아야 하는 경기 상태. 하나의 프로퍼티로 복제해서 항상 일관된 상태로 도착한다. */
USTRUCT(BlueprintType)
struct FJGMatchSnapshot
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Match")
	EJGMatchPhase Phase = EJGMatchPhase::WaitingPlayers;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Match")
	int32 ScoreA = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Match")
	int32 ScoreB = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Match")
	EJGTeam ServingTeam = EJGTeam::None;

	/** 득점마다 1씩 늘어나는 라운드 번호 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Match")
	int32 RoundNumber = 0;

	/** 현재 단계가 끝나는 서버 월드 시간. 시간 제한이 없는 단계면 0이다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Match")
	double PhaseEndServerTime = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Match")
	EJGTeam LastScoringTeam = EJGTeam::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Match")
	EJGPointReason LastPointReason = EJGPointReason::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Match")
	EJGTeam WinnerTeam = EJGTeam::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Match")
	EJGMatchEndReason EndReason = EJGMatchEndReason::None;
};

#pragma endregion
