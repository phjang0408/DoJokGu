#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Curves/CurveFloat.h"
#include "JGBalanceData.generated.h"

/**
 *  모든 조정 수치를 한곳에 모은 에셋(cm, 초). 값은 초기 실험치이며 공식 족구 규격이 아니다.
 *  이 클래스로 DA_Balance_Default를 만들고 Project Settings > Game > Jokgu에 지정한다.
 */
UCLASS(BlueprintType)
class UJGBalanceData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	UJGBalanceData();

#pragma region 접근과 변환

	/** 설정된 밸런스 에셋을 반환하고, 지정된 에셋이 없으면 클래스 기본값을 반환한다 */
	static const UJGBalanceData* Get();

	/** 설정된 밸런스 에셋에 대한 블루프린트 접근 */
	UFUNCTION(BlueprintPure, Category="Jokgu|Balance", DisplayName="Get Jokgu Balance Data")
	static UJGBalanceData* GetBalanceData();

	/** 정규화된 커브를 계산한다. 키가 없는 커브는 입력을 그대로 반환한다. */
	static float EvaluateCurve(const FRuntimeFloatCurve& Curve, float InTime);

	/** 1-5 캐릭터 능력치(이동 속도, 킥 파워, 리치)의 배율 */
	UFUNCTION(BlueprintPure, Category="Jokgu|Balance")
	float GetStatMultiplier(int32 Stat) const;

	/** 정확도 능력치에 따른 최대 무작위 조준 오차(도) */
	UFUNCTION(BlueprintPure, Category="Jokgu|Balance")
	float GetAccuracyDeviationDeg(int32 AccuracyStat) const;

#pragma endregion

#pragma region 경기

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Match", meta=(ClampMin=1))
	int32 TargetScore = 7;

	/** 이기려면 상대보다 이만큼 앞서야 한다(듀스) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Match", meta=(ClampMin=1))
	int32 WinMargin = 2;

	/** 득점 후 위치를 정리하고 서브를 시작하기까지의 대기 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Match", meta=(ClampMin=0, Units="s"))
	float ServePrepareTime = 1.5f;

	/** 이 시간 안에 서브하지 않으면 상대가 득점한다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Match", meta=(ClampMin=0, Units="s"))
	float ServeTimeLimit = 10.0f;

	/** 득점이 결정된 뒤 다음 서브 준비까지의 대기 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Match", meta=(ClampMin=0, Units="s"))
	float PointEndTime = 1.0f;

#pragma endregion

#pragma region 코트

	/** X축 방향 코트 전체 길이. 선의 바깥 가장자리 기준이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Court", meta=(ClampMin=100, Units="cm"))
	float CourtLength = 1200.0f;

	/** Y축 방향 코트 전체 폭. 선의 바깥 가장자리 기준이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Court", meta=(ClampMin=100, Units="cm"))
	float CourtWidth = 800.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Court", meta=(ClampMin=10, Units="cm"))
	float NetHeight = 105.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Court", meta=(ClampMin=1, Units="cm"))
	float NetThickness = 6.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Court", meta=(ClampMin=1, Units="cm"))
	float CourtLineWidth = 5.0f;

	/** 선 바깥에서 플레이어가 움직일 수 있는 수비 여유 공간 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Court", meta=(ClampMin=0, Units="cm"))
	float PlayerAreaMargin = 100.0f;

	/** 네트에서 서브 위치까지의 거리(자기 코트 안) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Court", meta=(ClampMin=0, Units="cm"))
	float ServeSpotDistanceFromNet = 500.0f;

	/** 네트에서 받는 플레이어 시작 위치까지의 거리 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Court", meta=(ClampMin=0, Units="cm"))
	float ReceiveSpotDistanceFromNet = 350.0f;

	/** 플레이어를 놓는 코트 바닥 위 높이 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Court", meta=(ClampMin=0, Units="cm"))
	float PlayerSpawnHeight = 100.0f;

#pragma endregion

#pragma region 공

	/** 판정에 쓰는 충돌 반지름. 보이는 메시 크기와 다를 수 있다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ball", meta=(ClampMin=1, Units="cm"))
	float BallRadius = 13.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ball", meta=(ClampMin=0))
	float BallGravityScale = 1.0f;

	/** 튈 때 수직 속도가 남는 비율 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ball", meta=(ClampMin=0, ClampMax=1))
	float BallBounciness = 0.55f;

	/** 튈 때 수평 속도가 줄어드는 비율 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ball", meta=(ClampMin=0, ClampMax=1))
	float BallFriction = 0.3f;

	/** 서브할 때 공을 서브하는 사람 발 앞으로 이만큼 떨어뜨려 든다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ball", meta=(Units="cm"))
	float ServeBallForwardOffset = 70.0f;

	/** 서브하는 사람 발 위 공 중심 높이 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ball", meta=(Units="cm"))
	float ServeBallHeight = 60.0f;

	/** 공이 이 월드 Z(코트 기준)보다 내려가면 코트 밖 지면 접촉으로 친다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ball", meta=(Units="cm"))
	float OutOfPlayZ = -200.0f;

#pragma endregion

#pragma region 이동

	/** 능력치 3 기준 이동 속도. 실제 속도는 능력치 배율을 곱한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement", meta=(ClampMin=0, Units="CentimetersPerSecond"))
	float BaseMoveSpeed = 450.0f;

#pragma endregion

#pragma region 터치 입력

	/** 이 시간 안에, 드래그 데드존 안에서 손을 떼면 탭이다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input", meta=(ClampMin=0, Units="s"))
	float TapMaxDuration = 0.18f;

	/** 무시하는 드래그 거리(DPI 배율이 적용된 위젯 단위) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input", meta=(ClampMin=0))
	float DragDeadZone = 12.0f;

	/** 최대 파워에 도달하는 드래그 거리(DPI 배율이 적용된 위젯 단위) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input", meta=(ClampMin=1))
	float DragMaxLength = 140.0f;

	/** 공격 정면 방향 기준 좌우 조준 한계 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input", meta=(ClampMin=0, ClampMax=89, Units="deg"))
	float AimAngleLimitDeg = 35.0f;

	/** 확정된 공격이 공이 범위에 들어오기를 기다리는 시간 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input", meta=(ClampMin=0, Units="s"))
	float AttackInputBufferTime = 0.15f;

	/** 타격 후 같은 플레이어가 다시 칠 수 있을 때까지의 지연 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input", meta=(ClampMin=0, Units="s"))
	float AttackRecoveryTime = 0.25f;

	/** 이 값보다 낮은 파워는 약한 킥 동작을 재생한다 (동작만 바꾸고 속도는 바꾸지 않는다) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input", meta=(ClampMin=0, ClampMax=1))
	float WeakMotionMaxPower = 0.33f;

	/** 이 값 이상인 파워는 강한 킥 동작을 재생한다 (동작만 바꾸고 속도는 바꾸지 않는다) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input", meta=(ClampMin=0, ClampMax=1))
	float StrongMotionMinPower = 0.67f;

#pragma endregion

#pragma region 타격 범위

	/** 캐릭터에서 공 중심까지의 수평 거리 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hit", meta=(ClampMin=0, Units="cm"))
	float ReachHorizontal = 120.0f;

	/** 발 기준 공 중심의 최소 높이 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hit", meta=(Units="cm"))
	float ReachMinHeight = 0.0f;

	/** 발 기준 공 중심의 최대 높이 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hit", meta=(Units="cm"))
	float ReachMaxHeight = 130.0f;

#pragma endregion

#pragma region 샷 궤적

	/** 탭: 공이 상대 쪽 네트 뒤로 이만큼 떨어진 곳에 떨어진다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot|Tap", meta=(ClampMin=0, Units="cm"))
	float TapTargetDepth = 150.0f;

	/** 탭: 포물선이 네트 위를 이만큼 여유 있게 지난다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot|Tap", meta=(ClampMin=0, Units="cm"))
	float TapNetClearance = 35.0f;

	/** 탭: 좌우 목표를 사이드라인 안쪽으로 이만큼 들여 놓는다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot|Tap", meta=(ClampMin=0, Units="cm"))
	float TapLateralInset = 60.0f;

	/** 탭: 포물선을 풀 수 없을 때(예: 공이 네트 바로 앞) 쓰는 비행 시간 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot|Tap", meta=(ClampMin=0.1, Units="s"))
	float TapFallbackFlightTime = 0.9f;

	/** 드래그: 파워 0일 때의 수평 속도 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot|Drag", meta=(ClampMin=0, Units="CentimetersPerSecond"))
	float DragHorizontalSpeedAtZero = 350.0f;

	/** 드래그: 최대 파워일 때의 수평 속도 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot|Drag", meta=(ClampMin=0, Units="CentimetersPerSecond"))
	float DragHorizontalSpeedAtFull = 1100.0f;

	/** 파워 [0,1]을 위 두 수평 속도 사이의 보간 비율로 바꾼다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot|Drag")
	FRuntimeFloatCurve DragHorizontalSpeedCurve;

	/** 드래그: 파워 0일 때의 수직 속도. 약하게 차면 높고 느린 공이 된다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot|Drag", meta=(Units="CentimetersPerSecond"))
	float DragVerticalSpeedAtZero = 520.0f;

	/** 드래그: 최대 파워일 때의 수직 속도. 세게 차면 낮고 빠른 공이 된다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot|Drag", meta=(Units="CentimetersPerSecond"))
	float DragVerticalSpeedAtFull = 380.0f;

	/** 파워 [0,1]을 위 두 수직 속도 사이의 보간 비율로 바꾼다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot|Drag")
	FRuntimeFloatCurve DragVerticalSpeedCurve;

	/** 슬라이딩 수비: 공을 상대 쪽 네트 뒤로 이만큼 보낸다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot|Slide", meta=(ClampMin=0, Units="cm"))
	float SlideReturnTargetDepth = 300.0f;

	/** 슬라이딩 수비: 높고 느리게, 비행 시간을 길게 준다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot|Slide", meta=(ClampMin=0.1, Units="s"))
	float SlideReturnFlightTime = 1.6f;

#pragma endregion

#pragma region 슬라이딩

	/** 슬라이딩으로 미끄러지는 거리 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Slide", meta=(ClampMin=0, Units="cm"))
	float SlideDistance = 250.0f;

	/** 미끄러지는 구간의 길이. 이 동안 리치가 늘어난다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Slide", meta=(ClampMin=0.05, Units="s"))
	float SlideActiveTime = 0.35f;

	/** 미끄러진 뒤 다시 움직일 수 있을 때까지의 회복 시간 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Slide", meta=(ClampMin=0, Units="s"))
	float SlideRecoveryTime = 0.6f;

	/** 슬라이딩 중에만 쓰는 리치. 회복이 시작되면 원래대로 돌아간다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Slide", meta=(ClampMin=0, Units="cm"))
	float SlideReachHorizontal = 170.0f;

	/** 슬라이딩 중 칠 수 있는 공 중심의 최대 높이 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Slide", meta=(Units="cm"))
	float SlideReachMaxHeight = 80.0f;

#pragma endregion

#pragma region 카메라

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera", meta=(ClampMin=-89, ClampMax=0, Units="deg"))
	float CameraPitch = -42.0f;

	/** 초점에서 카메라까지의 거리 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera", meta=(ClampMin=100, Units="cm"))
	float CameraDistance = 2600.0f;

	/** 네트에서 카메라 초점까지의 거리(자기 쪽 방향). 0이면 네트 위를 본다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera", meta=(Units="cm"))
	float CameraFocusDistanceFromNet = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera", meta=(ClampMin=5, ClampMax=170, Units="deg"))
	float CameraFieldOfView = 70.0f;

	/** 이 범위 안의 좌우 이동에는 카메라가 움직이지 않는다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera", meta=(ClampMin=0, Units="cm"))
	float CameraLateralDeadZone = 100.0f;

	/** 데드존을 넘은 좌우 이동 중 카메라가 따라가는 비율 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera", meta=(ClampMin=0, ClampMax=1))
	float CameraLateralFollowRatio = 0.5f;

	/** 카메라 좌우 이동의 최대치 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera", meta=(ClampMin=0, Units="cm"))
	float CameraLateralMaxOffset = 200.0f;

	/** 목표 위치로 따라가는 보간 속도 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera", meta=(ClampMin=0))
	float CameraFollowInterpSpeed = 4.0f;

#pragma endregion

#pragma region 캐릭터 능력치

	/** 능력치(1-5)를 이동 속도, 킥 파워, 리치 배율로 바꾼다. 3 = 100%. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats")
	FRuntimeFloatCurve StatMultiplierCurve;

	/** 정확도 능력치(1-5)를 최대 무작위 조준 오차(도)로 바꾼다. 조작감부터 검증하려고 0에서 시작한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats")
	FRuntimeFloatCurve AccuracyDeviationCurve;

#pragma endregion
};
