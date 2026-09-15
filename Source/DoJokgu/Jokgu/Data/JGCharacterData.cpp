#include "Jokgu/Data/JGCharacterData.h"
#include "Animation/AnimMontage.h"

UAnimMontage* UJGCharacterData::GetMontageForMotion(EJGKickMotion Motion) const
{
	UAnimMontage* Montage = nullptr;

	switch (Motion)
	{
	case EJGKickMotion::Tap:    Montage = TapMontage; break;
	case EJGKickMotion::Weak:   Montage = WeakKickMontage; break;
	case EJGKickMotion::Medium: Montage = MediumKickMontage; break;
	case EJGKickMotion::Strong: Montage = StrongKickMontage; break;
	case EJGKickMotion::Serve:  Montage = ServeMontage; break;
	case EJGKickMotion::Slide:  return SlideMontage;
	}

	// the prototype can run with a single kick motion placed in the medium slot
	return Montage ? Montage : MediumKickMontage.Get();
}
