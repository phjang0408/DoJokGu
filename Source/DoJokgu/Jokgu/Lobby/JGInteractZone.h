#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JGInteractZone.generated.h"

class UBoxComponent;
class UTextRenderComponent;

/** 구역에 들어가면 HUD가 여는 패널 */
UENUM(BlueprintType)
enum class EJGZoneKind : uint8
{
	/** 코트 앞: 방 만들기·방 목록 */
	Match,
	/** 학생회관 입구: 볼륨·그래픽 */
	Settings
};

/**
 *  로비의 상호작용 구역. 로컬 플레이어의 캐릭터가 상자에 들어오면 로비 컨트롤러에 알리고,
 *  HUD는 그 종류의 버튼을 보여 준다. 머리 위 글자로 구역을 표시한다.
 */
UCLASS()
class AJGInteractZone : public AActor
{
	GENERATED_BODY()

public:

	AJGInteractZone();

	EJGZoneKind GetKind() const { return Kind; }

	/** HUD 버튼과 머리 위 글자에 쓰는 이름 */
	FText GetDisplayName() const;

protected:

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UBoxComponent> Box;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UTextRenderComponent> Label;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Zone")
	EJGZoneKind Kind = EJGZoneKind::Match;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** 로컬에서 조종하는 Pawn이면 그 로비 컨트롤러에 들어옴/나감을 알린다 */
	void NotifyLocalPawn(AActor* OtherActor, bool bEntered);
};
