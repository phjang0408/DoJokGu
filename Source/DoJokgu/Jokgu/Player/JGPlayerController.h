#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "JGPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UJGMatchHUDWidget;

/**
 *  로컬 입력 설정, HUD 생성, 그리고 Pawn에 묶이지 않는 경기 요청의 진입점.
 *  UI는 로컬 컨트롤러에서만 만든다.
 */
UCLASS()
class AJGPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:

#pragma region 설정

	/** 로컬 플레이어에 추가할 입력 매핑 컨텍스트 (예: IMC_Match) */
	UPROPERTY(EditAnywhere, Category="Input")
	TArray<TObjectPtr<UInputMappingContext>> DefaultMappingContexts;

	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> RematchAction;

	/** HUD 클래스. 비우면 C++ 기본 UJGMatchHUDWidget 레이아웃을 쓴다. */
	UPROPERTY(EditAnywhere, Category="UI")
	TSubclassOf<UJGMatchHUDWidget> HUDWidgetClass;

	/** 데스크톱에서도 가상 조이스틱 등 터치 조작을 보여 준다 */
	UPROPERTY(EditAnywhere, Config, Category="UI")
	bool bForceTouchControls = false;

	UPROPERTY(Transient)
	TObjectPtr<UJGMatchHUDWidget> HUDWidget;

#pragma endregion

public:

#pragma region 요청과 조회

	UFUNCTION(BlueprintCallable, Category="Match")
	void RequestRematch();

	/** 선택 화면이 생기기 전까지 쓰는 테스트 명령. 예: "JGCharacter LeeMinji" */
	UFUNCTION(Exec, BlueprintCallable, Category="Characters")
	void JGCharacter(FName CharacterId);

	/** 터치 인터페이스를 써야 하는 기기이거나 bForceTouchControls가 켜져 있으면 true */
	UFUNCTION(BlueprintPure, Category="UI")
	bool ShouldUseTouchControls() const;

	UFUNCTION(BlueprintPure, Category="UI")
	UJGMatchHUDWidget* GetHUDWidget() const { return HUDWidget; }

#pragma endregion

protected:

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

#pragma region 서버 RPC

	UFUNCTION(Server, Reliable)
	void ServerRequestRematch();

	UFUNCTION(Server, Reliable)
	void ServerSelectCharacter(FName CharacterId);

#pragma endregion
};
