#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/EngineBaseTypes.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "JGSessionSubsystem.generated.h"

class UNetDriver;
class FOnlineSessionSearch;

/** 접속 실패 종류. 안내 화면이 사유 문자열 대신 이것으로 문구를 고른다. */
UENUM(BlueprintType)
enum class EJGConnectionFailure : uint8
{
	None,
	/** 연결 중 상대(Host)가 나갔거나 응답이 끊겼다 */
	ConnectionLost,
	/** 방에 들어가지 못했다(정원 초과, 방이 사라짐 등) */
	JoinFailed,
	/** 게임 버전이 달라 접속할 수 없다 */
	VersionMismatch,
	/** 방을 만들지 못했다 */
	HostFailed,
	/** 맵 설정 누락 등 그 밖의 실패 */
	Other
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FJGConnectionFailedSignature, const FString&, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FJGMatchSearchFinishedSignature, bool, bSucceeded);

/** 방 목록 한 줄. ResultIndex로 JoinFoundMatch를 부른다. */
USTRUCT(BlueprintType)
struct FJGFoundMatch
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Jokgu|Session")
	FString HostName;

	UPROPERTY(BlueprintReadOnly, Category="Jokgu|Session")
	int32 PingMs = 0;

	UPROPERTY(BlueprintReadOnly, Category="Jokgu|Session")
	int32 OpenSlots = 0;

	UPROPERTY(BlueprintReadOnly, Category="Jokgu|Session")
	int32 ResultIndex = INDEX_NONE;
};

/**
 *  방의 수명: 호스트, 방 찾기, 참가, 나가기, 접속 실패. GameInstance와 함께 살며 경기 점수는 저장하지 않는다.
 *  같은 와이파이(LAN): 호스트가 Null 서브시스템 LAN 세션을 만든 뒤 경기 맵을 Listen Server로 열고,
 *  참가자는 LAN 검색 결과의 주소로 접속한다. 세션 서브시스템이 없으면 세션 없이 바로 연다.
 *  IP 주소 직접 참가는 개발용으로 남긴다. 다음 단계(EOS)도 같은 API 뒤에서 내부만 바꾼다.
 */
UCLASS()
class UJGSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	/** 엔진의 네트워크·이동 실패 이벤트를 구독한다 */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

#pragma region 방 흐름

	/** LAN 세션을 만들고 경기 맵을 Listen Server로 연다. Host는 플레이어이자 권한자다. */
	UFUNCTION(BlueprintCallable, Category="Jokgu|Session")
	void HostMatch(const FString& MapPath);

	/** 프로젝트 설정(Jokgu > Maps > MatchMap)의 경기 맵으로 HostMatch를 부른다 */
	UFUNCTION(BlueprintCallable, Category="Jokgu|Session")
	void HostDefaultMatch();

	/** 같은 네트워크의 방을 찾는다. 끝나면 OnMatchSearchFinished가 불린다. */
	UFUNCTION(BlueprintCallable, Category="Jokgu|Session")
	void FindLanMatches();

	UFUNCTION(BlueprintPure, Category="Jokgu|Session")
	bool IsSearching() const { return bSearching; }

	UFUNCTION(BlueprintPure, Category="Jokgu|Session")
	const TArray<FJGFoundMatch>& GetFoundMatches() const { return FoundMatches; }

	/** 마지막 검색 결과의 방에 참가한다 */
	UFUNCTION(BlueprintCallable, Category="Jokgu|Session")
	void JoinFoundMatch(int32 ResultIndex);

	/** 로컬 네트워크의 Host에 IP[:port]로 참가한다 (개발용) */
	UFUNCTION(BlueprintCallable, Category="Jokgu|Session")
	void JoinByAddress(const FString& Address);

	/** 나가기, 실패, Host 종료가 함께 쓰는 공통 퇴장 경로. 세션을 닫고 메뉴(로비) 맵을 연다. */
	UFUNCTION(BlueprintCallable, Category="Jokgu|Session")
	void LeaveToMenu(const FString& MenuMapPath);

	/** 프로젝트 설정의 로비 맵으로 LeaveToMenu를 부른다 */
	UFUNCTION(BlueprintCallable, Category="Jokgu|Session")
	void LeaveToLobby();

	UPROPERTY(BlueprintAssignable, Category="Jokgu|Session")
	FJGMatchSearchFinishedSignature OnMatchSearchFinished;

	/** 방 만들기·참가가 진행 중이면 true. 그동안 들어온 새 만들기·참가 요청은 무시한다(버튼 연타 방지). */
	UFUNCTION(BlueprintPure, Category="Jokgu|Session")
	bool IsBusy() const { return bBusy; }

#pragma endregion

#pragma region 실패 알림

	/** 마지막 실패 사유. 새로 호스트하거나 참가하면 비운다. */
	UFUNCTION(BlueprintPure, Category="Jokgu|Session")
	FString GetLastFailureReason() const { return LastFailureReason; }

	/** 마지막 실패 종류. 사유와 함께 비워진다. */
	UFUNCTION(BlueprintPure, Category="Jokgu|Session")
	EJGConnectionFailure GetLastFailureKind() const { return LastFailureKind; }

	/** 사유를 읽었으면 비운다(로비가 한 번만 보여 주도록) */
	UFUNCTION(BlueprintCallable, Category="Jokgu|Session")
	void ClearLastFailureReason() { LastFailureReason.Reset(); LastFailureKind = EJGConnectionFailure::None; }

	UPROPERTY(BlueprintAssignable, Category="Jokgu|Session")
	FJGConnectionFailedSignature OnConnectionFailed;

#pragma endregion

protected:

	IOnlineSessionPtr GetSessionInterface() const;

	/** 지난 세션이 남아 있으면 지운 뒤 Then을 부른다. 지우는 중에 또 부르면 이어서 할 일을 순서대로 덧붙인다. */
	void DestroySessionThen(TFunction<void()> Then);

	/** 엔진 실패 이벤트는 모든 GameInstance(PIE 창마다 하나)에 오므로 이 GameInstance의 월드인지 확인한다 */
	bool IsOwnWorld(const UWorld* World, const UNetDriver* NetDriver) const;

	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleFindSessionsComplete(bool bWasSuccessful);
	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful);

	void OpenListenMap(const FString& MapPath);

	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);

	/** 사유를 저장하고 로그를 남긴 뒤 OnConnectionFailed로 알린다. 진행 중이던 만들기·참가도 끝낸다. */
	void ReportFailure(EJGConnectionFailure Kind, const FString& Reason);

	static FString MapPathFromSettings(bool bMatchMap);

	FString LastFailureReason;
	EJGConnectionFailure LastFailureKind = EJGConnectionFailure::None;
	bool bBusy = false;
	bool bDestroying = false;
	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;

	FDelegateHandle CreateSessionHandle;
	FDelegateHandle FindSessionsHandle;
	FDelegateHandle JoinSessionHandle;
	FDelegateHandle DestroySessionHandle;

	TSharedPtr<FOnlineSessionSearch> SessionSearch;
	TArray<FJGFoundMatch> FoundMatches;
	bool bSearching = false;

	/** 세션 생성이 끝나면 열 맵 */
	FString PendingHostMap;

	/** 세션 삭제가 끝나면 이어서 할 일 */
	TFunction<void()> AfterDestroy;
};
