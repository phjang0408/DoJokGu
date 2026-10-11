#include "Jokgu/Player/JGRigAnimationLibrary.h"
#include "Animation/AnimMontage.h"

bool UJGRigAnimationLibrary::ConfigureContactMontage(UAnimMontage* Montage, float ContactSeconds)
{
	if (!Montage || !FMath::IsFinite(ContactSeconds) || ContactSeconds < 0.f || ContactSeconds >= Montage->GetPlayLength())
	{
		return false;
	}
	Montage->Modify();
	// 여러 번 실행해도 Contact 구간이 하나만 남도록 기존 구간을 지운다
	const int32 Existing = Montage->GetSectionIndex(TEXT("Contact"));
	if (Existing != INDEX_NONE)
	{
		Montage->DeleteAnimCompositeSection(Existing);
	}
	Montage->AddAnimCompositeSection(TEXT("Contact"), ContactSeconds);
	// 서버가 확정한 타격은 접촉 순간부터 재생한다: 준비 동작을 블렌드인하면 공보다 눈에 띄게 늦어진다.
	Montage->BlendIn.SetBlendTime(0.f);
	Montage->BlendOut.SetBlendTime(0.12f);
	Montage->MarkPackageDirty();
	return true;
}
