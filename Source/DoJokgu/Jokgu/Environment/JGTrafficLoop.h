#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JGTrafficLoop.generated.h"

class USplineComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** 경로를 도는 차 한 대 */
USTRUCT(BlueprintType)
struct FJGTrafficCar
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Traffic")
	TObjectPtr<UStaticMesh> Mesh;

	/** 시작할 때 경로 위 거리(cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Traffic")
	float StartDistance = 0.0f;

	/** 경로 위 지점(진행 방향이 X) 기준 메시의 상대 위치·회전. 메시의 앞 방향과 높이를 맞춘다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Traffic")
	FTransform MeshOffset;
};

/**
 *  학교 둘레 도로를 반복해서 도는 차들(환경 연출). 닫힌 Spline 하나에 차 여러 대가 같은 속도로 달려 간격이 유지된다.
 *  충돌·복제 없이 각 기기에서 따로 움직인다. 경로와 차는 Tools/Campus 정리 스크립트가 도로에 있던 차로 만든다.
 */
UCLASS()
class AJGTrafficLoop : public AActor
{
	GENERATED_BODY()

public:

	AJGTrafficLoop();

	virtual void Tick(float DeltaSeconds) override;

protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USplineComponent> Path;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Traffic")
	TArray<FJGTrafficCar> Cars;

	/** cm/s. 900이면 약 32km/h */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Traffic", meta=(ClampMin="0"))
	float Speed = 900.0f;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

private:

	/** 차마다 하나. 저장하지 않고 시작할 때(에디터에서는 구성할 때) 만든다. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> CarComponents;

	float TravelledDistance = 0.0f;

	void RebuildCarComponents();
	void UpdateCarTransforms();
};
