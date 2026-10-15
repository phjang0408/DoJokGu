#include "Jokgu/Online/JGSessionSubsystem.h"
#include "Jokgu/Data/JGGameSettings.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystemUtils.h"
#include "DoJokgu.h"

namespace
{
	/**
	 *  Host와 상대 한 명. Listen Server에서는 Host 자신도 세션에 등록되어 빈자리를 하나 쓰므로
	 *  상대 수(1)만 넣으면 방을 열자마자 빈자리가 0이 되어 LAN 검색에 응답하지 않는다.
	 *  정원 검사는 게임 모드의 PreLogin도 한 번 더 한다.
	 */
	constexpr int32 MatchPlayers = 2;
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
		ReportFailure(EJGConnectionFailure::Other, TEXT("No match map set in Project Settings > Jokgu > Maps."));
		return;
	}

	HostMatch(MapPath);
}

void UJGSessionSubsystem::HostMatch(const FString& MapPath)
{
	if (bBusy)
	{
		UE_LOG(LogDoJokgu, Log, TEXT("Host ignored: another host or join is in progress"));
		return;
	}

	ClearLastFailureReason();
	bBusy = true;

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
		Settings.NumPublicConnections = MatchPlayers;

		CreateSessionHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
			FOnCreateSessionCompleteDelegate::CreateUObject(this, &UJGSessionSubsystem::HandleCreateSessionComplete));

		UE_LOG(LogDoJokgu, Log, TEXT("Creating LAN session for %s"), *PendingHostMap);

		if (!Sessions->CreateSession(0, NAME_GameSession, Settings))
		{
			Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionHandle);
			ReportFailure(EJGConnectionFailure::HostFailed, TEXT("Could not start creating the LAN session."));
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
		ReportFailure(EJGConnectionFailure::HostFailed, TEXT("Could not create the LAN session."));
		return;
	}

	OpenListenMap(PendingHostMap);
}

void UJGSessionSubsystem::OpenListenMap(const FString& MapPath)
{
	UE_LOG(LogDoJokgu, Log, TEXT("Hosting listen server on %s"), *MapPath);
	bBusy = false;
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

	if (bJoinFirstAfterSearch)
	{
		bJoinFirstAfterSearch = false;
		if (FoundMatches.Num() > 0)
		{
			JoinFoundMatch(FoundMatches[0].ResultIndex);
		}
	}
}

void UJGSessionSubsystem::JoinFoundMatch(int32 ResultIndex)
{
	if (bBusy)
	{
		UE_LOG(LogDoJokgu, Log, TEXT("Join ignored: another host or join is in progress"));
		return;
	}

	ClearLastFailureReason();

	const IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions || !SessionSearch.IsValid() || !SessionSearch->SearchResults.IsValidIndex(ResultIndex))
	{
		ReportFailure(EJGConnectionFailure::JoinFailed, TEXT("That match is no longer in the list. Refresh and try again."));
		return;
	}

	bBusy = true;

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
			ReportFailure(EJGConnectionFailure::JoinFailed, TEXT("Could not start joining the match."));
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
		ReportFailure(EJGConnectionFailure::JoinFailed, FString::Printf(TEXT("Could not join the match (%s)."), LexToString(Result)));
		return;
	}

	JoinByAddress(Address);
}

void UJGSessionSubsystem::JoinByAddress(const FString& Address)
{
	ClearLastFailureReason();

	APlayerController* PlayerController = GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
	if (!PlayerController)
	{
		ReportFailure(EJGConnectionFailure::Other, TEXT("No local player controller to travel with."));
		return;
	}

	// 여기부터의 실패(정원 초과, 응답 없음)는 엔진의 네트워크·이동 실패 이벤트로 들어온다
	UE_LOG(LogDoJokgu, Log, TEXT("Joining %s"), *Address);
	bBusy = false;
	PlayerController->ClientTravel(Address, TRAVEL_Absolute);
}

#pragma endregion

#pragma region 나가기

void UJGSessionSubsystem::LeaveToLobby()
{
	const FString MapPath = MapPathFromSettings(false);
	if (MapPath.IsEmpty())
	{
		ReportFailure(EJGConnectionFailure::Other, TEXT("No lobby map set in Project Settings > Jokgu > Maps."));
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
	// 이미 지우는 중이면 완료 뒤에 이어서 하도록 덧붙인다(나가기 중 접속 끊김 등)
	if (bDestroying)
	{
		AfterDestroy = [First = MoveTemp(AfterDestroy), Second = MoveTemp(Then)]()
		{
			if (First)
			{
				First();
			}
			if (Second)
			{
				Second();
			}
		};
		return;
	}

	const IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions || !Sessions->GetNamedSession(NAME_GameSession))
	{
		if (Then)
		{
			Then();
		}
		return;
	}

	bDestroying = true;
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
	bDestroying = false;
	TFunction<void()> Then = MoveTemp(AfterDestroy);
	AfterDestroy = nullptr;

	if (Then)
	{
		Then();
	}
}

#pragma endregion

#pragma region 개발용 콘솔 명령

#if !UE_BUILD_SHIPPING
/** UI 없이 패키지 두 개로 LAN 흐름을 시험한다. 예: -ExecCmds="jg.Session.Host" / -ExecCmds="jg.Session.FindAndJoin" */
struct FJGSessionConsoleCommands
{
	static UJGSessionSubsystem* Get(UWorld* World)
	{
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UJGSessionSubsystem>() : nullptr;
	}

	static void Host(UWorld* World)
	{
		if (UJGSessionSubsystem* Session = Get(World))
		{
			Session->HostDefaultMatch();
		}
	}

	static void FindAndJoin(UWorld* World)
	{
		if (UJGSessionSubsystem* Session = Get(World))
		{
			Session->bJoinFirstAfterSearch = true;
			Session->FindLanMatches();
		}
	}
};

static FAutoConsoleCommandWithWorld GJGSessionHostCommand(
	TEXT("jg.Session.Host"), TEXT("LAN 방을 만들고 경기 맵을 Listen Server로 연다 (개발용)"),
	FConsoleCommandWithWorldDelegate::CreateStatic(&FJGSessionConsoleCommands::Host));

static FAutoConsoleCommandWithWorld GJGSessionFindAndJoinCommand(
	TEXT("jg.Session.FindAndJoin"), TEXT("같은 네트워크의 LAN 방을 찾아 첫 방에 참가한다 (개발용)"),
	FConsoleCommandWithWorldDelegate::CreateStatic(&FJGSessionConsoleCommands::FindAndJoin));
#endif

#pragma endregion

#pragma region 실패 알림

bool UJGSessionSubsystem::IsOwnWorld(const UWorld* World, const UNetDriver* NetDriver) const
{
	const UWorld* FailedWorld = World ? World : (NetDriver ? NetDriver->GetWorld() : nullptr);
	if (FailedWorld)
	{
		return FailedWorld->GetGameInstance() == GetGameInstance();
	}

	// 접속 중(PendingNetGame) 실패는 월드가 없으므로 NetDriver로 GameInstance를 찾는다
	if (NetDriver && GEngine)
	{
		if (const FWorldContext* Context = GEngine->GetWorldContextFromPendingNetGameNetDriver(NetDriver))
		{
			return Context->OwningGameInstance == GetGameInstance();
		}
	}

	// 알 수 없으면 자기 것으로 본다. 패키지 실행에는 GameInstance가 하나뿐이다.
	return true;
}

void UJGSessionSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	if (!IsOwnWorld(World, NetDriver))
	{
		return;
	}

	const bool bIsServer = NetDriver && NetDriver->GetNetMode() != NM_Client;
	EJGConnectionFailure Kind = EJGConnectionFailure::Other;

	switch (FailureType)
	{
	case ENetworkFailure::ConnectionLost:
	case ENetworkFailure::ConnectionTimeout:
	case ENetworkFailure::FailureReceived:
		// Host에서는 상대 한 명의 연결이 끊긴 것이다. 게임 모드의 Logout이 경기를 끝내고, 방은 새 상대를 기다린다.
		if (bIsServer)
		{
			UE_LOG(LogDoJokgu, Log, TEXT("A client connection ended (%s): %s"), ENetworkFailure::ToString(FailureType), *ErrorString);
			return;
		}
		Kind = EJGConnectionFailure::ConnectionLost;
		break;

	case ENetworkFailure::PendingConnectionFailure:
		Kind = EJGConnectionFailure::JoinFailed;
		break;

	case ENetworkFailure::OutdatedClient:
	case ENetworkFailure::OutdatedServer:
		Kind = EJGConnectionFailure::VersionMismatch;
		break;

	case ENetworkFailure::NetDriverAlreadyExists:
	case ENetworkFailure::NetDriverCreateFailure:
	case ENetworkFailure::NetDriverListenFailure:
		Kind = EJGConnectionFailure::HostFailed;
		break;

	default:
		break;
	}

	ReportFailure(Kind, FString::Printf(TEXT("Network failure (%s): %s"), ENetworkFailure::ToString(FailureType), *ErrorString));

	// 엔진이 기본 맵(로비)으로 돌려보내는 동안 남은 세션을 지워 둔다. 다음 방 만들기·참가가 깨끗한 상태에서 시작한다.
	DestroySessionThen(nullptr);
}

void UJGSessionSubsystem::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	if (!IsOwnWorld(World, nullptr))
	{
		return;
	}

	ReportFailure(EJGConnectionFailure::JoinFailed, FString::Printf(TEXT("Travel failure (%s): %s"), ETravelFailure::ToString(FailureType), *ErrorString));
	DestroySessionThen(nullptr);
}

void UJGSessionSubsystem::ReportFailure(EJGConnectionFailure Kind, const FString& Reason)
{
	bBusy = false;
	LastFailureKind = Kind;
	LastFailureReason = Reason;
	UE_LOG(LogDoJokgu, Warning, TEXT("%s (%s)"), *Reason, *UEnum::GetValueAsString(Kind));
	OnConnectionFailed.Broadcast(Reason);
}

#pragma endregion
