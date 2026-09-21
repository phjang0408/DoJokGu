#include "Jokgu/Player/JGRigAnimationLibrary.h"
#include "Animation/AnimMontage.h"

bool UJGRigAnimationLibrary::ConfigureContactMontage(UAnimMontage* Montage, float ContactSeconds)
{
	if (!Montage || !FMath::IsFinite(ContactSeconds) || ContactSeconds < 0.f || ContactSeconds >= Montage->GetPlayLength())
	{
		return false;
	}
	Montage->Modify();
	const int32 Existing = Montage->GetSectionIndex(TEXT("Contact"));
	if (Existing != INDEX_NONE)
	{
		Montage->DeleteAnimCompositeSection(Existing);
	}
	Montage->AddAnimCompositeSection(TEXT("Contact"), ContactSeconds);
	// A server-confirmed hit starts at contact: blending in a windup would visibly lag the ball.
	Montage->BlendIn.SetBlendTime(0.f);
	Montage->BlendOut.SetBlendTime(0.12f);
	Montage->MarkPackageDirty();
	return true;
}
