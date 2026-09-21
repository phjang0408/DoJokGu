#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "JGRigAnimationLibrary.generated.h"

class UAnimMontage;

/** Small native bridge for montage section timing, which is not exposed in UE Python. */
UCLASS()
class DOJOKGU_API UJGRigAnimationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="Jokgu|Animation")
	static bool ConfigureContactMontage(UAnimMontage* Montage, float ContactSeconds);
};
