#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "JGCharacterSelectWidget.generated.h"

class UHorizontalBox;
class UTextBlock;
class AJGGameStateBase;
class AJGPlayerState;
class UJGCharacterData;

DECLARE_DELEGATE_OneParam(FJGRosterButtonPicked, FName /*CharacterId*/);

/** 로스터 선수 하나를 고르는 버튼. 눌리면 자기 CharacterId를 알린다. */
UCLASS()
class UJGRosterButton : public UButton
{
	GENERATED_BODY()

public:

	void Init(FName InCharacterId, FJGRosterButtonPicked InOnPicked);

	FName GetCharacterId() const { return CharacterId; }

protected:

	UFUNCTION()
	void HandleClicked();

	FName CharacterId;
	FJGRosterButtonPicked OnPicked;
};

/**
 *  경기 맵 대기 단계의 선택 화면: 로스터 버튼, 고른 선수의 능력치, 준비 버튼, 양쪽 준비 상태.
 *  게임 모드가 서버에서 검증하므로 이 위젯은 요청만 보내고, 표시는 복제된 PlayerState를 그대로 따른다.
 *  위젯 트리가 비어 있으면 기본 레이아웃을 코드로 만든다.
 */
UCLASS()
class UJGCharacterSelectWidget : public UUserWidget
{
	GENERATED_BODY()

protected:

#pragma region 바인딩 위젯

	UPROPERTY(BlueprintReadOnly, Category="Select", meta=(BindWidgetOptional))
	TObjectPtr<UHorizontalBox> RosterBox;

	UPROPERTY(BlueprintReadOnly, Category="Select", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> InfoText;

	UPROPERTY(BlueprintReadOnly, Category="Select", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(BlueprintReadOnly, Category="Select", meta=(BindWidgetOptional))
	TObjectPtr<UButton> ReadyButton;

	UPROPERTY(BlueprintReadOnly, Category="Select", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ReadyLabel;

#pragma endregion

	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void BuildDefaultLayout();

	/** 로스터가 복제되면 한 번 버튼을 만든다 */
	void TryBuildRosterButtons(const AJGGameStateBase& GameState);

	/** 선택 강조, 능력치, 준비 버튼 글자, 상태 줄을 복제된 값으로 다시 쓴다 */
	void RefreshFromState(const AJGGameStateBase& GameState);

	void HandleRosterPicked(FName CharacterId);

	UFUNCTION()
	void HandleReadyClicked();

	AJGPlayerState* GetLocalPlayerState() const;

	/** 상대 PlayerState. 아직 없으면 nullptr. */
	const AJGPlayerState* FindOpponentState(const AJGGameStateBase& GameState) const;

	static FText DescribeCharacter(const UJGCharacterData* Data);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UJGRosterButton>> RosterButtons;
};
