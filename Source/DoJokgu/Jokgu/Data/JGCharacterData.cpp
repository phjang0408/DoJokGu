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
	case EJGKickMotion::Slide:  return SlideMontage; // 슬라이딩은 킥 동작으로 대신하지 않는다
	}

	// 프로토타입은 중간 슬롯에 킥 동작 하나만 넣어도 돌아간다
	return Montage ? Montage : MediumKickMontage.Get();
}
