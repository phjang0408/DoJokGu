#include "Jokgu/Gameplay/JGShotCalculator.h"
#include "Jokgu/Gameplay/JGCourt.h"
#include "Jokgu/Data/JGBalanceData.h"

#pragma region 입력 해석

float UJGShotCalculator::ComputeDragPower(float DragLength)
{
	const UJGBalanceData* Balance = UJGBalanceData::Get();
	const float Range = FMath::Max(Balance->DragMaxLength - Balance->DragDeadZone, KINDA_SMALL_NUMBER);
	return FMath::Clamp((DragLength - Balance->DragDeadZone) / Range, 0.0f, 1.0f);
}

float UJGShotCalculator::ComputeAimAngle(const FVector2D& DragDelta)
{
	const UJGBalanceData* Balance = UJGBalanceData::Get();
	// 화면 위쪽(-Y)이 0도, 오른쪽(+X)이 양수 각도
	const float Angle = FMath::RadiansToDegrees(FMath::Atan2(DragDelta.X, -DragDelta.Y));
	return FMath::Clamp(Angle, -Balance->AimAngleLimitDeg, Balance->AimAngleLimitDeg);
}

bool UJGShotCalculator::ResolveTouchGesture(const FVector2D& DragDelta, float HeldSeconds, FJGAttackInput& OutInput)
{
	const UJGBalanceData* Balance = UJGBalanceData::Get();
	const float DragLength = DragDelta.Size();

	OutInput = FJGAttackInput();

	if (DragLength <= Balance->DragDeadZone)
	{
		// 움직이지 않고 짧게 누르면 탭, 길게 누르면 취소
		if (HeldSeconds > Balance->TapMaxDuration)
		{
			return false;
		}

		OutInput.Type = EJGAttackType::Tap;
		return true;
	}

	OutInput.Type = EJGAttackType::Drag;
	OutInput.Power = ComputeDragPower(DragLength);
	OutInput.AimAngleDeg = ComputeAimAngle(DragDelta);
	return true;
}

EJGKickMotion UJGShotCalculator::SelectKickMotion(const FJGAttackInput& Input, bool bIsServe)
{
	if (bIsServe)
	{
		return EJGKickMotion::Serve;
	}

	if (Input.Type == EJGAttackType::Tap)
	{
		return EJGKickMotion::Tap;
	}

	const UJGBalanceData* Balance = UJGBalanceData::Get();

	if (Input.Power < Balance->WeakMotionMaxPower)
	{
		return EJGKickMotion::Weak;
	}

	return Input.Power < Balance->StrongMotionMinPower ? EJGKickMotion::Medium : EJGKickMotion::Strong;
}

#pragma endregion

#pragma region 궤적 계산

FVector UJGShotCalculator::GetAimDirection(const FVector& AttackForward, float AimAngleDeg)
{
	return AttackForward.GetSafeNormal2D().RotateAngleAxis(AimAngleDeg, FVector::UpVector);
}

FVector UJGShotCalculator::ComputeDragVelocity(const FVector& AttackForward, float AimAngleDeg, float Power, float KickPowerMultiplier)
{
	const UJGBalanceData* Balance = UJGBalanceData::Get();
	const float ClampedPower = FMath::Clamp(Power, 0.0f, 1.0f);
	const float ClampedAngle = FMath::Clamp(AimAngleDeg, -Balance->AimAngleLimitDeg, Balance->AimAngleLimitDeg);

	const float HorizontalAlpha = UJGBalanceData::EvaluateCurve(Balance->DragHorizontalSpeedCurve, ClampedPower);
	const float VerticalAlpha = UJGBalanceData::EvaluateCurve(Balance->DragVerticalSpeedCurve, ClampedPower);

	// 킥 파워 능력치 배율은 수평 속도에만 곱하고, 수직 속도는 캐릭터와 상관없이 같다
	const float HorizontalSpeed = FMath::Lerp(Balance->DragHorizontalSpeedAtZero, Balance->DragHorizontalSpeedAtFull, HorizontalAlpha) * KickPowerMultiplier;
	const float VerticalSpeed = FMath::Lerp(Balance->DragVerticalSpeedAtZero, Balance->DragVerticalSpeedAtFull, VerticalAlpha);

	return GetAimDirection(AttackForward, ClampedAngle) * HorizontalSpeed + FVector::UpVector * VerticalSpeed;
}

FVector UJGShotCalculator::ComputeVelocityToTarget(const FVector& Start, const FVector& Target, float FlightTime, float GravityZ)
{
	// 수평은 등속, 수직은 z = Vz*t + 0.5*g*t^2 를 Vz에 대해 푼다
	const float Time = FMath::Max(FlightTime, KINDA_SMALL_NUMBER);
	FVector Velocity = (Target - Start) / Time;
	Velocity.Z = (Target.Z - Start.Z) / Time - 0.5f * GravityZ * Time;
	return Velocity;
}

bool UJGShotCalculator::SolveArcThroughPoint(const FVector& Start, const FVector& Via, const FVector& Target, float GravityZ, FVector& OutVelocity)
{
	const double Gravity = -GravityZ;
	if (Gravity <= 0.0)
	{
		return false;
	}

	const FVector Flat(Target.X - Start.X, Target.Y - Start.Y, 0.0);
	const double TargetDistance = Flat.Size();
	if (TargetDistance < 1.0)
	{
		return false;
	}

	// Via는 Start-Target 수평선 위에 투영해서 쓰며, 둘 사이에 있어야 한다
	const FVector Direction = Flat / TargetDistance;
	const double ViaDistance = FVector::DotProduct(Via - Start, Direction);
	if (ViaDistance <= 1.0 || ViaDistance >= TargetDistance - 1.0)
	{
		return false;
	}

	// 수평 거리 d에서의 높이: z(d) = A*d - K*d^2, 여기서 A = Vz/Vh, K = g / (2*Vh^2)
	// Via와 Target 두 점을 지나는 조건으로 A와 K를 푼다
	const double ViaHeight = Via.Z - Start.Z;
	const double TargetHeight = Target.Z - Start.Z;

	const double K = (TargetDistance * ViaHeight - ViaDistance * TargetHeight)
		/ (ViaDistance * TargetDistance * (TargetDistance - ViaDistance));

	// K <= 0이면 Via가 Start-Target을 잇는 직선보다 높지 않아서 중력 포물선으로는 지날 수 없다
	if (K <= 0.0)
	{
		return false;
	}

	const double A = (TargetHeight + K * TargetDistance * TargetDistance) / TargetDistance;
	const double HorizontalSpeed = FMath::Sqrt(Gravity / (2.0 * K));

	OutVelocity = Direction * HorizontalSpeed + FVector::UpVector * (A * HorizontalSpeed);
	return true;
}

#pragma endregion

#pragma region 샷별 속도

FVector UJGShotCalculator::ComputeTapVelocity(const AJGCourt& Court, EJGTeam HitterTeam, const FVector& BallLocation, float GravityZ)
{
	const UJGBalanceData* Balance = UJGBalanceData::Get();
	const FTransform& CourtTransform = Court.GetActorTransform();

	const double ForwardSign = Court.GetAttackSign(HitterTeam);
	const FVector BallLocal = CourtTransform.InverseTransformPosition(BallLocation);
	const double LateralLimit = FMath::Max(0.0, Court.GetHalfWidth() - Balance->TapLateralInset);
	const double Lateral = FMath::Clamp(BallLocal.Y, -LateralLimit, LateralLimit);

	// 경유점은 네트 바로 위(공 반지름 + 여유), 목표는 상대 쪽 네트 뒤 바닥(공 중심 높이)
	const FVector ViaLocal(0.0, Lateral, Balance->NetHeight + Balance->BallRadius + Balance->TapNetClearance);
	const FVector TargetLocal(ForwardSign * Balance->TapTargetDepth, Lateral, Balance->BallRadius);

	const FVector Target = CourtTransform.TransformPosition(TargetLocal);

	// 공이 네트에 너무 가깝거나 높아서 포물선을 풀 수 없으면 고정 비행 시간으로 보낸다
	FVector Velocity;
	if (!SolveArcThroughPoint(BallLocation, CourtTransform.TransformPosition(ViaLocal), Target, GravityZ, Velocity))
	{
		Velocity = ComputeVelocityToTarget(BallLocation, Target, Balance->TapFallbackFlightTime, GravityZ);
	}

	return Velocity;
}

FVector UJGShotCalculator::ComputeSlideReturnVelocity(const AJGCourt& Court, EJGTeam HitterTeam, const FVector& BallLocation, float GravityZ)
{
	const UJGBalanceData* Balance = UJGBalanceData::Get();
	const FVector TargetLocal(Court.GetAttackSign(HitterTeam) * Balance->SlideReturnTargetDepth, 0.0, Balance->BallRadius);
	const FVector Target = Court.GetActorTransform().TransformPosition(TargetLocal);

	return ComputeVelocityToTarget(BallLocation, Target, Balance->SlideReturnFlightTime, GravityZ);
}

FVector UJGShotCalculator::ComputeAttackVelocity(const AJGCourt& Court, EJGTeam HitterTeam, const FVector& BallLocation, const FJGAttackInput& Input, float KickPowerMultiplier, float AimDeviationDeg, float GravityZ)
{
	FVector Velocity = FVector::ZeroVector;

	switch (Input.Type)
	{
	case EJGAttackType::Tap:
		Velocity = ComputeTapVelocity(Court, HitterTeam, BallLocation, GravityZ);
		break;

	case EJGAttackType::Drag:
		Velocity = ComputeDragVelocity(Court.GetAttackForward(HitterTeam), Input.AimAngleDeg, Input.Power, KickPowerMultiplier);
		break;

	default:
		break;
	}

	// 정확도 오차는 서버가 한 번 정하고 수평 방향에만 적용한다
	if (!FMath::IsNearlyZero(AimDeviationDeg))
	{
		Velocity = Velocity.RotateAngleAxis(AimDeviationDeg, FVector::UpVector);
	}

	return Velocity;
}

#pragma endregion
