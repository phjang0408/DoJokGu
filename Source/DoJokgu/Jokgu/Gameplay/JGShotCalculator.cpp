#include "Jokgu/Gameplay/JGShotCalculator.h"
#include "Jokgu/Gameplay/JGCourt.h"
#include "Jokgu/Data/JGBalanceData.h"

float UJGShotCalculator::ComputeDragPower(float DragLength)
{
	const UJGBalanceData* Balance = UJGBalanceData::Get();
	const float Range = FMath::Max(Balance->DragMaxLength - Balance->DragDeadZone, KINDA_SMALL_NUMBER);
	return FMath::Clamp((DragLength - Balance->DragDeadZone) / Range, 0.0f, 1.0f);
}

float UJGShotCalculator::ComputeAimAngle(const FVector2D& DragDelta)
{
	const UJGBalanceData* Balance = UJGBalanceData::Get();
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
		// short press without movement is a tap, a long press without movement is cancelled
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

	const float HorizontalSpeed = FMath::Lerp(Balance->DragHorizontalSpeedAtZero, Balance->DragHorizontalSpeedAtFull, HorizontalAlpha) * KickPowerMultiplier;
	const float VerticalSpeed = FMath::Lerp(Balance->DragVerticalSpeedAtZero, Balance->DragVerticalSpeedAtFull, VerticalAlpha);

	return GetAimDirection(AttackForward, ClampedAngle) * HorizontalSpeed + FVector::UpVector * VerticalSpeed;
}

FVector UJGShotCalculator::ComputeVelocityToTarget(const FVector& Start, const FVector& Target, float FlightTime, float GravityZ)
{
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

	const FVector Direction = Flat / TargetDistance;
	const double ViaDistance = FVector::DotProduct(Via - Start, Direction);
	if (ViaDistance <= 1.0 || ViaDistance >= TargetDistance - 1.0)
	{
		return false;
	}

	// height along the flat distance d: z(d) = A*d - K*d^2, with A = Vz/Vh and K = g / (2*Vh^2)
	const double ViaHeight = Via.Z - Start.Z;
	const double TargetHeight = Target.Z - Start.Z;

	const double K = (TargetDistance * ViaHeight - ViaDistance * TargetHeight)
		/ (ViaDistance * TargetDistance * (TargetDistance - ViaDistance));

	if (K <= 0.0)
	{
		return false;
	}

	const double A = (TargetHeight + K * TargetDistance * TargetDistance) / TargetDistance;
	const double HorizontalSpeed = FMath::Sqrt(Gravity / (2.0 * K));

	OutVelocity = Direction * HorizontalSpeed + FVector::UpVector * (A * HorizontalSpeed);
	return true;
}

FVector UJGShotCalculator::ComputeTapVelocity(const AJGCourt& Court, EJGTeam HitterTeam, const FVector& BallLocation, float GravityZ)
{
	const UJGBalanceData* Balance = UJGBalanceData::Get();
	const FTransform& CourtTransform = Court.GetActorTransform();

	const double ForwardSign = Court.GetAttackSign(HitterTeam);
	const FVector BallLocal = CourtTransform.InverseTransformPosition(BallLocation);
	const double LateralLimit = FMath::Max(0.0, Court.GetHalfWidth() - Balance->TapLateralInset);
	const double Lateral = FMath::Clamp(BallLocal.Y, -LateralLimit, LateralLimit);

	const FVector ViaLocal(0.0, Lateral, Balance->NetHeight + Balance->BallRadius + Balance->TapNetClearance);
	const FVector TargetLocal(ForwardSign * Balance->TapTargetDepth, Lateral, Balance->BallRadius);

	const FVector Target = CourtTransform.TransformPosition(TargetLocal);

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

	// accuracy deviation is decided once by the server and applied to the horizontal direction only
	if (!FMath::IsNearlyZero(AimDeviationDeg))
	{
		Velocity = Velocity.RotateAngleAxis(AimDeviationDeg, FVector::UpVector);
	}

	return Velocity;
}
