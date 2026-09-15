#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Jokgu/Core/JGTypes.h"
#include "JGCourt.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UMaterialInterface;

/**
 *  Placeholder court: floor, lines, net and player area blockers built from engine basic shapes.
 *  Judgement uses the numbers from UJGBalanceData, never the visual meshes.
 *  Local space: origin under the net center, +X toward team B's baseline. Team A owns -X and attacks toward +X.
 */
UCLASS()
class AJGCourt : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> Root;

	/** Single floor collision so bounces are never counted twice by overlapping tiles */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UBoxComponent> GroundCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UBoxComponent> NetCollision;

	/** Blocks pawns only: keeps players on their side and inside the defensive margin */
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

protected:

	/** Half size of the square floor around the court */
	UPROPERTY(EditAnywhere, Category="Court", meta=(ClampMin=0, Units="cm"))
	float GroundHalfExtent = 2500.0f;

	/** Height of the invisible walls that keep players in their area */
	UPROPERTY(EditAnywhere, Category="Court", meta=(ClampMin=0, Units="cm"))
	float PlayerBlockerHeight = 400.0f;

	/** Base material with a "Color" vector parameter (defaults to the engine BasicShapeMaterial) */
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

public:

	AJGCourt();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Re-applies sizes from the balance data to collisions and visuals */
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Court")
	void ApplyDimensions();

	UFUNCTION(BlueprintPure, Category="Court")
	float GetHalfLength() const;

	UFUNCTION(BlueprintPure, Category="Court")
	float GetHalfWidth() const;

	UFUNCTION(BlueprintPure, Category="Court")
	float GetNetHeight() const;

	/** +1 if the team attacks toward local +X (team A), -1 otherwise */
	UFUNCTION(BlueprintPure, Category="Court")
	float GetAttackSign(EJGTeam Team) const;

	/** World direction from the team's court toward the opponent court */
	UFUNCTION(BlueprintPure, Category="Court")
	FVector GetAttackForward(EJGTeam Team) const;

	UFUNCTION(BlueprintPure, Category="Court")
	EJGCourtZone ClassifyWorldLocation(const FVector& WorldLocation) const;

	/** Start transform for a team: the serve spot for the server, the receive spot otherwise */
	UFUNCTION(BlueprintPure, Category="Court")
	FTransform GetStartTransform(EJGTeam Team, bool bIsServing) const;

	/** Center of a team's half, on the floor */
	UFUNCTION(BlueprintPure, Category="Court")
	FVector GetTeamCourtCenter(EJGTeam Team) const;

	/** Fixed high camera behind a team's half, with a limited lateral follow of the given location */
	UFUNCTION(BlueprintPure, Category="Court")
	FTransform GetCameraViewTransform(EJGTeam Team, const FVector& FollowWorldLocation) const;

protected:

	UBoxComponent* CreatePlayerBlocker(FName Name);
	UStaticMeshComponent* CreateVisualMesh(FName Name, UStaticMesh* Mesh);
	void PlaceBox(UBoxComponent* Box, const FVector& LocalCenter, const FVector& HalfExtent) const;
	void PlaceCube(UStaticMeshComponent* Mesh, const FVector& LocalCenter, const FVector& Size) const;
	void ApplyColor(UStaticMeshComponent* Mesh, const FLinearColor& Color);
	void DrawDebugCourt() const;
};
