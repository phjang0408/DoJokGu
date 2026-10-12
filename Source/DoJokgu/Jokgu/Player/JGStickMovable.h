#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "JGStickMovable.generated.h"

UINTERFACE(MinimalAPI)
class UJGStickMovable : public UInterface
{
	GENERATED_BODY()
};

/** 가상 조이스틱이 움직일 수 있는 Pawn. 경기 캐릭터와 로비 캐릭터가 함께 쓴다. */
class IJGStickMovable
{
	GENERATED_BODY()

public:

	/** 화면 기준 스틱 값. Right는 오른쪽, Forward는 화면 위쪽이 +다. */
	virtual void DoStickMove(float Right, float Forward) = 0;
};
