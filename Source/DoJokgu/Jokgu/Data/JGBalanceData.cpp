#include "Jokgu/Data/JGBalanceData.h"
#include "Jokgu/Data/JGGameSettings.h"

namespace
{
	void AddLinearKey(FRuntimeFloatCurve& Curve, float Time, float Value)
	{
		FRichCurve* RichCurve = Curve.GetRichCurve();
		const FKeyHandle Handle = RichCurve->AddKey(Time, Value);
		RichCurve->SetKeyInterpMode(Handle, RCIM_Linear);
	}
}

UJGBalanceData::UJGBalanceData()
{
	AddLinearKey(DragHorizontalSpeedCurve, 0.0f, 0.0f);
	AddLinearKey(DragHorizontalSpeedCurve, 1.0f, 1.0f);

	AddLinearKey(DragVerticalSpeedCurve, 0.0f, 0.0f);
	AddLinearKey(DragVerticalSpeedCurve, 1.0f, 1.0f);

	// initial experiment from the plan: stat 2 / 3 / 5 -> 95% / 100% / 110%
	AddLinearKey(StatMultiplierCurve, 1.0f, 0.90f);
	AddLinearKey(StatMultiplierCurve, 2.0f, 0.95f);
	AddLinearKey(StatMultiplierCurve, 3.0f, 1.00f);
	AddLinearKey(StatMultiplierCurve, 4.0f, 1.05f);
	AddLinearKey(StatMultiplierCurve, 5.0f, 1.10f);

	AddLinearKey(AccuracyDeviationCurve, 1.0f, 0.0f);
	AddLinearKey(AccuracyDeviationCurve, 5.0f, 0.0f);
}

const UJGBalanceData* UJGBalanceData::Get()
{
	if (const UJGGameSettings* Settings = GetDefault<UJGGameSettings>())
	{
		if (const UJGBalanceData* Data = Settings->BalanceData.LoadSynchronous())
		{
			return Data;
		}
	}

	return GetDefault<UJGBalanceData>();
}

UJGBalanceData* UJGBalanceData::GetBalanceData()
{
	return const_cast<UJGBalanceData*>(Get());
}

float UJGBalanceData::EvaluateCurve(const FRuntimeFloatCurve& Curve, float InTime)
{
	const FRichCurve* RichCurve = Curve.GetRichCurveConst();
	if (!RichCurve || RichCurve->GetNumKeys() == 0)
	{
		return InTime;
	}

	return RichCurve->Eval(InTime);
}

float UJGBalanceData::GetStatMultiplier(int32 Stat) const
{
	const FRichCurve* RichCurve = StatMultiplierCurve.GetRichCurveConst();
	if (!RichCurve || RichCurve->GetNumKeys() == 0)
	{
		return 1.0f;
	}

	return RichCurve->Eval(static_cast<float>(FMath::Clamp(Stat, 1, 5)));
}

float UJGBalanceData::GetAccuracyDeviationDeg(int32 AccuracyStat) const
{
	const FRichCurve* RichCurve = AccuracyDeviationCurve.GetRichCurveConst();
	if (!RichCurve || RichCurve->GetNumKeys() == 0)
	{
		return 0.0f;
	}

	return FMath::Max(0.0f, RichCurve->Eval(static_cast<float>(FMath::Clamp(AccuracyStat, 1, 5))));
}
