#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "JGRigAnimationLibrary.generated.h"

class UAnimMontage;

/** UE Python에 노출되지 않은 몽타주 구간 시간 설정을 위한 작은 C++ 연결 함수 모음 */
UCLASS()
class DOJOKGU_API UJGRigAnimationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/**
	 *  몽타주에 ContactSeconds 위치의 "Contact" 구간을 만들고(있으면 다시 만든다) 블렌드 시간을 맞춘다.
	 *  에디터 에셋 수정용이며, 시간이 몽타주 길이 밖이면 false를 반환한다.
	 */
	UFUNCTION(BlueprintCallable, Category="Jokgu|Animation")
	static bool ConfigureContactMontage(UAnimMontage* Montage, float ContactSeconds);
};
