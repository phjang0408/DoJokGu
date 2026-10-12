#include "Jokgu/Online/JGSessionSubsystem.h"
#include "Jokgu/Data/JGGameSettings.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystemUtils.h"
#include "DoJokgu.h"

namespace
{
	/** 상대 한 명만 받는다. 정원 검사는 게임 모드의 PreLogin도 한 번 더 한다. */
	constexpr int32 OpponentSlots = 1;
	constexpr int32 MaxSearchResults = 20;
}

#pragma region 초기화

void UJGSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UJGSessionSubsystem::HandleNetworkFailure);
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &UJGSessionSubsystem::HandleTravelFailure);
	}
}

void UJGSessionSubsystem::Deinitialize()
{
	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}

	if (const IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionHandle);
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsHandle);
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionHandle);
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionHandle);
	}

	Super::Deinitialize();
}

IOnlineSessionPtr UJGSessionSubsystem::GetSessionInterface() const
{
	// PIE 인스턴스마다 서브시스템이 따로 있으므로 월드 기준으로 찾는다
	const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	return World ? Online::GetSessionInterface(World) : nullptr;
}

FString UJGSessionSubsystem::MapPathFromSettings(bool bMatchMap)
{
	const UJGGameSettings* Settings = GetDefault<UJGGameSettings>();
	const TSoftObjectPtr<UWorld>& Map = bMatchMap ? Settings->MatchMap : Settings->LobbyMap;
	return Map.IsNull() ? FString() : Map.GetLongPackageName();
}

#pragma endregion

#pragma region 호스트

void UJGSessionSubsystem::HostDefaultMatch()
{
	const FString MapPath = MapPathFromSettings(true);
	if (MapPath.IsEmpty())
	{
		ReportFailure(TEXT("No match map set in Project Settings > Jokgu > Maps."));
		return;
	}

	HostMatch(MapPath);
}

void UJGSessionSubsystem::HostMatch(const FString& MapPath)
{
	LastFailureReason.Reset();

	const IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions)
	{
		UE_LOG(LogDoJokgu, Warning, TEXT("No online session interface. Hosting without a LAN session (not listed)."));
		OpenListenMap(MapPath);
		return;
	}

	PendingHostMap = MapPath;

	// 지난 경기의 세션이 남아 있으면 같은 이름으로 만들 수 없다
	DestroySessionThen([this]()
	{
		const IOnlineSessionPtr Sessions = GetSessionInterface();
		if (!Sessions)
		{
			return;
		}

		FOnlineSessionSettings Settings;
		Settings.bIsLANMatch = true;
		Settings.bShouldAdvertise = true;
		Settings.bUsesPresence = false;
		Settings.bAllowJoinInProgress = true;
		Settings.NumPublicConnections = OpponentSlots;

		CreateSessionHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
			FOnCreateSessionCompleteDelegate::CreateUObject(this, &UJGSessionSubsystem::HandleCreateSessionComplete));

		UE_LOG(LogDoJokgu, Log, TEXT("Creating LAN session for %s"), *PendingHostMap);

		if (!Sessions->CreateSession(0, NAME_GameSession, Settings))
		{
			Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionHandle);
			ReportFailure(TEXT("Could not start creating the LAN session."));
		}
	});
}

void UJGSessionSubsystem::HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (const IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionHandle);
	}

	if (!bWasSuccessful)
	{
		ReportFailure(TEXT("Could not create the LAN session."));
		return;
	}

	OpenListenMap(PendingHostMap);
}

void UJGSessionSubsystem::OpenListenMap(const FString& MapPath)
{
	UE_LOG(LogDoJokgu, Log, TEXT("Hosting listen server on %s"), *MapPath);
	UGameplayStatics::OpenLevel(this, FName(*MapPath), true, TEXT("listen"));
}

#pragma endregion

#pragma region 방 찾기와 참가

void UJGSessionSubsystem::FindLanMatches()
{
	const IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions || bSearching)
	{
		if (!Sessions)
		{
			OnMatchSearchFinished.Broadcast(false);
		}
		return;
	}

	SessionSearch = MakeShared<FOnlineSessionSearch>();
	SessionSearch->bIsLanQuery = true;
	SessionSearch->MaxSearchResults = MaxSearchResults;

	FindSessionsHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &UJGSessionSubsystem::HandleFindSessionsComplete));

	bSearching = true;
	FoundMatches.Reset();

	if (!Sessions->FindSessions(0, SessionSearch.ToSharedRef()))
	{
		HandleFindSessionsComplete(false);
	}
}

void UJGSessionSubsystem::HandleFindSessionsComplete(bool bWasSuccessful)
{
	if (const IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsHandle);
	}

	bSearching = false;
	FoundMatches.Reset();

	if (bWasSuccessful && SessionSearch.IsValid())
	{
		for (int32 Index = 0; Index < SessionSearch->SearchResults.Num(); ++Index)
		{
			const FOnlineSessionSearchResult& Result = SessionSearch->SearchResults[Index];

			// 상대가 이미 들어온 방은 목록에 보여 주지 않는다
			if (!Result.IsValid() || Result.Session.NumOpenPublicConnections <= 0)
			{
				continue;
			}

			FJGFoundMatch& Match = FoundMatches.AddDefaulted_GetRef();
			Match.HostName = Result.Session.OwningUserName;
			Match.PingMs = Result.PingInMs;
			Match.OpenSlots = Result.Session.NumOpenPublicConnections;
			Match.ResultIndex = Index;
		}
	}

	UE_LOG(LogDoJokgu, Log, TEXT("LAN search %s: %d open match(es)"), bWasSuccessful ? TEXT("done") : TEXT("failed"), FoundMatches.Num());
	OnMatchSearchFinished.Broadcast(bWasSuccessful);
}

void UJGSessionSubsystem::JoinFoundMatch(int32 ResultIndex)
{
	LastFailureReason.Reset();

	const IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions || !SessionSearch.IsValid() || !SessionSearch->SearchResults.IsValidIndex(ResultIndex))
	{
		ReportFailure(TEXT("That match is no longer in the list. Refresh and try again."));
		return;
	}

	const FOnlineSessionSearchResult Result = SessionSearch->SearchResults[ResultIndex];

	DestroySessionThen([this, Result]()
	{
		const IOnlineSessionPtr Sessions = GetSessionInterface();
		if (!Sessions)
		{
			return;
		}

		JoinSessionHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(
			FOnJoinSessionCompleteDelegate::CreateUObject(this, &UJGSessionSubsystem::HandleJoinSessionComplete));

		if (!Sessions->JoinSession(0, NAME_GameSession, Result))
		{
			Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionHandle);
			ReportFailure(TEXT("Could not start joining the match."));
		}
	});
}

void UJGSessionSubsystem::HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	const IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions)
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionHandle);
	}

	FString Address;
	if (Result != EOnJoinSessionCompleteResult::Success || !Sessions || !Sessions->GetResolvedConnectString(SessionName, Address))
	{
		ReportFailure(FString::Printf(TEXT("Could not join the match (%s)."), LexToString(Result)));
		return;
	}

	JoinByAddress(Address);
}

void UJGSessionSubsystem::JoinByAddress(const FString& Address)
{
	LastFailureReason.Reset();

	APlayerController* PlayerController = GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
	if (!PlayerController)
	{
		ReportFailure(TEXT("No local player controller to travel with."));
		return;
	}

	UE_LOG(LogDoJokgu, Log, TEXT("Joining %s"), *Address);
	PlayerController->ClientTravel(Address, TRAVEL_Absolute);
}

#pragma endregion

#pragma region 나가기

void UJGSessionSubsystem::LeaveToLobby()
{
	const FString MapPath = MapPathFromSettings(false);
	if (MapPath.IsEmpty())
	{
		ReportFailure(TEXT("No lobby map set in Project Settings > Jokgu > Maps."));
		return;
	}

	LeaveToMenu(MapPath);
}

void UJGSessionSubsystem::LeaveToMenu(const FString& MenuMapPath)
{
	UE_LOG(LogDoJokgu, Log, TEXT("Leaving match to %s"), *MenuMapPath);

	// 세션을 닫아야 방 목록에서 사라지고 다음에 같은 이름으로 다시 만들 수 있다.
	// ?listen 없이 레벨을 열면 Listen Server나 클라이언트 연결이 닫힌다.
	DestroySessionThen([this, MenuMapPath]()
	{
		UGameplayStatics::OpenLevel(this, FName(*MenuMapPath), true);
	});
}

void UJGSessionSubsystem::DestroySessionThen(TFunction<void()> Then)
{
	const IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions || !Sessions->GetNamedSession(NAME_GameSession))
	{
		Then();
		return;
	}

	AfterDestroy = MoveTemp(Then);
	DestroySessionHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
		FOnDestroySessionCompleteDelegate::CreateUObject(this, &UJGSessionSubsystem::HandleDestroySessionComplete));

	if (!Sessions->DestroySession(NAME_GameSession))
	{
		HandleDestroySessionComplete(NAME_GameSession, false);
	}
}

void UJGSessionSubsystem::HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (const IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionHandle);
	}

	if (!bWasSuccessful)
	{
		UE_LOG(LogDoJokgu, Warning, TEXT("Could not destroy session %s; continuing anyway"), *SessionName.ToString());
	}

	// 실패해도 다음 단계(맵 이동 등)는 진행한다. 남은 세션은 다음 호스트 때 다시 지운다.
	TFunction<void()> Then = MoveTemp(AfterDestroy);
	AfterDestroy = nullptr;

	if (Then)
	{
		Then();
	}
}

#pragma endregion

#pragma region 실패 알림

void UJGSessionSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	ReportFailure(FString::Printf(TEXT("Network failure (%s): %s"), ENetworkFailure::ToString(FailureType), *ErrorString));
}

void UJGSessionSubsystem::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	ReportFailure(FString::Printf(TEXT("Travel failure (%s): %s"), ETravelFailure::ToString(FailureType), *ErrorString));
}

void UJGSessionSubsystem::ReportFailure(const FString& Reason)
{
	LastFailureReason = Reason;
	UE_LOG(LogDoJokgu, Warning, TEXT("%s"), *Reason);
	OnConnectionFailed.Broadcast(Reason);
}

#pragma endregion
