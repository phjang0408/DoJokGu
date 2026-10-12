#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Jokgu/Lobby/JGInteractZone.h"
#include "JGLobbyPlayerController.generated.h"

class UInputMappingContext;
class UJGLobbyHUDWidget;

/**
 *  로비 컨트롤러: 입력 매핑, 로비 HUD 생성, 지금 서 있는 상호작용 구역 추적.
 *  로비는 혼자 쓰는 Standalone 맵이라 서버 RPC가 없다.
 */
UCLASS()
class AJGLobbyPlayerController : public APlayerController
{
	GENERATED_BODY()

public:

	/** 구역 상자에 들어가거나 나올 때 AJGInteractZone이 부른다 */
	void SetZoneOverlap(AJGInteractZone* Zone, bool bEntered);

	/** 터치 인터페이스를 써야 하는 기기이거나 bForceTouchControls가 켜져 있으면 true */
	UFUNCTION(BlueprintPure, Category="UI")
	bool ShouldUseTouchControls() const;

	UFUNCTION(BlueprintPure, Category="UI")
	UJGLobbyHUDWidget* GetHUDWidget() const { return HUDWidget; }

protected:

	/** 로컬 플레이어에 추가할 입력 매핑 컨텍스트 (경기와 같은 IMC_Match를 쓰면 WASD 이동이 같다) */
	UPROPERTY(EditAnywhere, Category="Input")
	TArray<TObjectPtr<UInputMappingContext>> DefaultMappingContexts;

	/** HUD 클래스. 비우면 C++ 기본 UJGLobbyHUDWidget 레이아웃을 쓴다. */
	UPROPERTY(EditAnywhere, Category="UI")
	TSubclassOf<UJGLobbyHUDWidget> HUDWidgetClass;

	/** 데스크톱에서도 가상 조이스틱을 보여 준다 */
	UPROPERTY(EditAnywhere, Config, Category="UI")
	bool bForceTouchControls = false;

	UPROPERTY(Transient)
	TObjectPtr<UJGLobbyHUDWidget> HUDWidget;

	/** 겹친 구역들. 마지막에 들어간 구역의 버튼을 보여 준다. */
	TArray<TWeakObjectPtr<AJGInteractZone>> OverlappingZones;

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	void RefreshZoneButton();
};
