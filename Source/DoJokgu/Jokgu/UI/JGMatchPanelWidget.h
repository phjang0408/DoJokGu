#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Jokgu/UI/JGSettingsPanelWidget.h"
#include "JGMatchPanelWidget.generated.h"

class UTextBlock;
class UVerticalBox;

DECLARE_DELEGATE_OneParam(FJGIndexButtonClicked, int32 /*Index*/);

/** 목록 한 줄 버튼. 눌리면 자기 번호를 알린다. */
UCLASS()
class UJGIndexButton : public UButton
{
	GENERATED_BODY()

public:

	void Init(int32 InIndex, FJGIndexButtonClicked InOnClicked);

protected:

	UFUNCTION()
	void HandleClicked();

	int32 Index = INDEX_NONE;
	FJGIndexButtonClicked OnIndexClicked;
};

/**
 *  경기 패널: 방 만들기, 같은 와이파이의 방 목록(검색 중·없음·목록), 새로고침, 참가, 닫기.
 *  열 때마다 목록을 새로 찾는다. 실제 동작은 UJGSessionSubsystem이 한다.
 *  위젯 트리가 비어 있으면 기본 레이아웃을 코드로 만든다.
 */
UCLASS()
class UJGMatchPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** 닫기 버튼을 누르면 알린다. 패널을 숨기는 것은 소유 HUD가 한다. */
	FJGPanelClosedSignature OnCloseRequested;

	/** 패널을 열 때 부른다: 목록을 새로 찾는다 */
	void Refresh();

protected:

#pragma region 바인딩 위젯

	UPROPERTY(BlueprintReadOnly, Category="Match", meta=(BindWidgetOptional))
	TObjectPtr<UButton> HostButton;

	UPROPERTY(BlueprintReadOnly, Category="Match", meta=(BindWidgetOptional))
	TObjectPtr<UButton> RefreshButton;

	UPROPERTY(BlueprintReadOnly, Category="Match", meta=(BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(BlueprintReadOnly, Category="Match", meta=(BindWidgetOptional))
	TObjectPtr<UVerticalBox> MatchList;

	UPROPERTY(BlueprintReadOnly, Category="Match", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

#pragma endregion

	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

	void BuildDefaultLayout();

	/** 마지막 검색 결과로 목록 버튼을 다시 만든다 */
	void RebuildList();

	void SetStatus(const FString& Status);

	void HandleMatchPicked(int32 ResultIndex);

	UFUNCTION()
	void HandleSearchFinished(bool bSucceeded);

	UFUNCTION()
	void HandleConnectionFailed(const FString& Reason);

	UFUNCTION()
	void HandleHostClicked();

	UFUNCTION()
	void HandleRefreshClicked();

	UFUNCTION()
	void HandleCloseClicked();

	/** 방 만들기·참가를 누른 뒤에는 맵 이동 전까지 버튼을 다시 받지 않는다 */
	void SetBusy(bool bBusy);
};
