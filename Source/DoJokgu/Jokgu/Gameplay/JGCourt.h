#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Jokgu/Core/JGTypes.h"
#include "JGCourt.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UMaterialInterface;

/**
 *  임시 코트: 엔진 기본 도형으로 만든 바닥, 선, 네트, 플레이어 영역 차단벽.
 *  판정은 보이는 메시가 아니라 언제나 UJGBalanceData의 수치를 쓴다.
 *  로컬 좌표: 원점은 네트 중심 아래, +X는 B팀 엔드라인 쪽. A팀은 -X를 쓰고 +X로 공격한다.
 */
UCLASS()
class AJGCourt : public AActor
{
	GENERATED_BODY()

#pragma region 충돌 컴포넌트

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> Root;

	/** 바닥 충돌을 하나로 만들어 겹친 타일 때문에 바운드가 두 번 세어지지 않게 한다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UBoxComponent> GroundCollision;

	/** 공만 튕기는 네트 충돌. 판정용 폭은 코트 폭보다 양쪽 50cm 넓다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UBoxComponent> NetCollision;

	/** Pawn만 막는다: 플레이어를 자기 진영과 수비 여유 공간 안에 둔다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UBoxComponent> NetPlayerBlocker;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UBoxComponent> SideBlockerLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UBoxComponent> SideBlockerRight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UBoxComponent> BackBlockerA;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UBoxComponent> BackBlockerB;

#pragma endregion

#pragma region 임시 표시 메시

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components|Visual", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> GroundMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components|Visual", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> CourtSurfaceMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components|Visual", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> LineSideLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components|Visual", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> LineSideRight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components|Visual", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> LineBackA;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components|Visual", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> LineBackB;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components|Visual", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> LineCenter;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components|Visual", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> NetMesh;

#pragma endregion

protected:

#pragma region 설정

	/** 코트 주변 정사각형 바닥의 반 크기. 코트와 여유 공간보다 작으면 자동으로 늘린다. */
	UPROPERTY(EditAnywhere, Category="Court", meta=(ClampMin=0, Units="cm"))
	float GroundHalfExtent = 2500.0f;

	/** 플레이어를 영역 안에 가두는 보이지 않는 벽의 높이 */
	UPROPERTY(EditAnywhere, Category="Court", meta=(ClampMin=0, Units="cm"))
	float PlayerBlockerHeight = 400.0f;

	/** 레벨이 자체 코트 아트를 가질 때(예: 학교 맵) 끈다. 충돌과 판정은 그대로다. */
	UPROPERTY(EditAnywhere, Category="Court|Visual")
	bool bShowPlaceholderVisuals = true;

	/** "Color" 벡터 파라미터가 있는 기본 재질 (기본값은 엔진 BasicShapeMaterial) */
	UPROPERTY(EditAnywhere, Category="Court|Visual")
	TObjectPtr<UMaterialInterface> PlaceholderMaterial;

	UPROPERTY(EditAnywhere, Category="Court|Visual")
	FLinearColor GroundColor = FLinearColor(0.20f, 0.28f, 0.18f);

	UPROPERTY(EditAnywhere, Category="Court|Visual")
	FLinearColor CourtColor = FLinearColor(0.55f, 0.42f, 0.28f);

	UPROPERTY(EditAnywhere, Category="Court|Visual")
	FLinearColor LineColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, Category="Court|Visual")
	FLinearColor NetColor = FLinearColor(0.05f, 0.05f, 0.08f);

#pragma endregion

public:

	AJGCourt();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	/** 밸런스 데이터의 크기를 충돌체와 표시 메시에 다시 적용한다 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Court")
	void ApplyDimensions();

#pragma region 판정 좌표 조회

	UFUNCTION(BlueprintPure, Category="Court")
	float GetHalfLength() const;

	UFUNCTION(BlueprintPure, Category="Court")
	float GetHalfWidth() const;

	UFUNCTION(BlueprintPure, Category="Court")
	float GetNetHeight() const;

	/** 팀이 로컬 +X로 공격하면(A팀) +1, 아니면 -1 */
	UFUNCTION(BlueprintPure, Category="Court")
	float GetAttackSign(EJGTeam Team) const;

	/** 팀의 코트에서 상대 코트를 향하는 월드 방향 */
	UFUNCTION(BlueprintPure, Category="Court")
	FVector GetAttackForward(EJGTeam Team) const;

	/** 월드 위치를 코트 로컬로 바꿔 A 코트, B 코트, 밖 중 하나로 분류한다 */
	UFUNCTION(BlueprintPure, Category="Court")
	EJGCourtZone ClassifyWorldLocation(const FVector& WorldLocation) const;

#pragma endregion

#pragma region 시작 위치와 카메라

	/** 팀의 시작 트랜스폼: 서브하는 쪽은 서브 위치, 아니면 받는 위치. 상대 코트를 바라본다. */
	UFUNCTION(BlueprintPure, Category="Court")
	FTransform GetStartTransform(EJGTeam Team, bool bIsServing) const;

	/** 팀 진영 절반의 바닥 중심 */
	UFUNCTION(BlueprintPure, Category="Court")
	FVector GetTeamCourtCenter(EJGTeam Team) const;

	/** 팀 진영 뒤쪽 위의 고정 카메라. 주어진 위치를 좌우로만 제한적으로 따라간다. */
	UFUNCTION(BlueprintPure, Category="Court")
	FTransform GetCameraViewTransform(EJGTeam Team, const FVector& FollowWorldLocation) const;

#pragma endregion

protected:

#pragma region 구성 도우미

	/** Pawn만 막는 차단벽 박스를 만든다 (생성자 전용) */
	UBoxComponent* CreatePlayerBlocker(FName Name);

	/** 충돌 없는 표시용 메시를 만든다 (생성자 전용) */
	UStaticMeshComponent* CreateVisualMesh(FName Name, UStaticMesh* Mesh);

	void PlaceBox(UBoxComponent* Box, const FVector& LocalCenter, const FVector& HalfExtent) const;

	/** 엔진 기본 큐브를 로컬 중심과 전체 크기(cm)에 맞춰 놓는다 */
	void PlaceCube(UStaticMeshComponent* Mesh, const FVector& LocalCenter, const FVector& Size) const;

	void ApplyColor(UStaticMeshComponent* Mesh, const FLinearColor& Color);
	void DrawDebugCourt() const;

#pragma endregion
};
