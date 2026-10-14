#include "Jokgu/Core/JGGameModeBase.h"
#include "Jokgu/Core/JGGameStateBase.h"
#include "Jokgu/Core/JGMatchRules.h"
#include "Jokgu/Core/JGPlayerState.h"
#include "Jokgu/Data/JGBalanceData.h"
#include "Jokgu/Data/JGCharacterData.h"
#include "Jokgu/Data/JGCharacterRoster.h"
#include "Jokgu/Gameplay/JGBall.h"
#include "Jokgu/Gameplay/JGCourt.h"
#include "Jokgu/Player/JGCharacter.h"
#include "Jokgu/Player/JGPlayerController.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "DoJokgu.h"

#pragma region 엔진 오버라이드

AJGGameModeBase::AJGGameModeBase()
{
	GameStateClass = AJGGameStateBase::StaticClass();
	PlayerStateClass = AJGPlayerState::StaticClass();
	PlayerControllerClass = AJGPlayerController::StaticClass();
	DefaultPawnClass = AJGCharacter::StaticClass();
	BallClass = AJGBall::StaticClass();
	FallbackCourtClass = AJGCourt::StaticClass();
}

void AJGGameModeBase::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);

	if (ErrorMessage.IsEmpty() && GetNumPlayers() >= MaxMatchPlayers)
	{
		ErrorMessage = TEXT("Match is full.");
	}
}

void AJGGameModeBase::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	EnsureCourtAndBall();

	// Super가 RestartPlayer로 스폰하기 전에 팀과 캐릭터를 정해야 시작 위치와 외형이 맞는다
	if (AJGPlayerState* PlayerState = NewPlayer ? NewPlayer->GetPlayerState<AJGPlayerState>() : nullptr)
	{
		if (PlayerState->GetTeam() == EJGTeam::None)
		{
			PlayerState->SetTeam(FindFreeTeam());
		}

		// 로비에서 캐릭터를 골라 왔으면(CopyProperties) 그대로 쓴다
		const UJGCharacterData* StartingData = GetStartingCharacterData(PlayerState->GetTeam());
		if (PlayerState->GetCharacterId().IsNone() && StartingData)
		{
			PlayerState->SetCharacterId(StartingData->CharacterId);
		}

		// 준비는 이 경기 맵의 대기 단계에서 새로 받는다
		PlayerState->SetReady(false);
	}

	Super::HandleStartingNewPlayer_Implementation(NewPlayer);

	TryStartMatch();
}

void AJGGameModeBase::RestartPlayer(AController* NewPlayer)
{
	if (!NewPlayer || NewPlayer->IsPendingKillPending())
	{
		return;
	}

	EnsureCourtAndBall();

	const AJGPlayerState* PlayerState = NewPlayer->GetPlayerState<AJGPlayerState>();
	const AJGGameStateBase* JGGameState = GetJGGameState();

	// 팀이 없는 관전자 등은 엔진 기본 PlayerStart 규칙을 따른다
	if (!Court || !PlayerState || PlayerState->GetTeam() == EJGTeam::None || !JGGameState)
	{
		Super::RestartPlayer(NewPlayer);
		return;
	}

	const bool bIsServing = PlayerState->GetTeam() == JGGameState->GetServingTeam();
	RestartPlayerAtTransform(NewPlayer, Court->GetStartTransform(PlayerState->GetTeam(), bIsServing));

	if (AJGCharacter* Character = Cast<AJGCharacter>(NewPlayer->GetPawn()))
	{
		Character->SetCharacterData(UJGCharacterRoster::Get()->FindOrDefault(PlayerState->GetCharacterId()));
	}
}

void AJGGameModeBase::Logout(AController* Exiting)
{
	AJGPlayerState* PlayerState = Exiting ? Exiting->GetPlayerState<AJGPlayerState>() : nullptr;
	const bool bWasOnTeam = PlayerState && PlayerState->GetTeam() != EJGTeam::None;

	// 팀을 비워 두어야 다음에 들어오는 플레이어가 이 자리를 받는다
	if (PlayerState)
	{
		PlayerState->SetTeam(EJGTeam::None);
	}

	Super::Logout(Exiting);

	const AJGGameStateBase* JGGameState = GetJGGameState();
	if (bPracticeMode || !bWasOnTeam || !JGGameState)
	{
		return;
	}

	const FJGMatchSnapshot Match = JGGameState->GetMatch();
	const bool bAlreadyEnded = Match.Phase == EJGMatchPhase::MatchEnd && Match.EndReason == EJGMatchEndReason::PlayerLeft;

	if (Match.Phase == EJGMatchPhase::WaitingPlayers || bAlreadyEnded)
	{
		return;
	}

	// 첫 버전: 나가면 경기가 끝나고, 남은 플레이어에게 사유를 보여 준다
	if (CountTeamPlayers(Exiting) < GetRequiredPlayers())
	{
		EndMatch(EJGMatchEndReason::PlayerLeft, EJGTeam::None);
	}
}

void AJGGameModeBase::StartPlay()
{
	EnsureCourtAndBall();

	Super::StartPlay();

	// 연습 모드는 서브 없이 바로 랠리 상태로 두어 공 발사기의 공을 칠 수 있게 한다
	if (bPracticeMode)
	{
		SetPhase(EJGMatchPhase::Rally, 0.0f);
		return;
	}

	TryStartMatch();
}

#pragma endregion

#pragma region 코트와 공, 경기 시작

void AJGGameModeBase::EnsureCourtAndBall()
{
	UWorld* World = GetWorld();

	if (!Court)
	{
		for (TActorIterator<AJGCourt> It(World); It; ++It)
		{
			Court = *It;
			break;
		}
	}

	if (!Court)
	{
		UE_LOG(LogDoJokgu, Warning, TEXT("No AJGCourt in the level. Spawning a fallback court at the origin."));

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Court = World->SpawnActor<AJGCourt>(FallbackCourtClass ? FallbackCourtClass.Get() : AJGCourt::StaticClass(), FTransform::Identity, Params);
	}

	if (!Ball && Court)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		const FVector SpawnLocation = Court->GetActorLocation() + FVector(0.0, 0.0, 200.0);
		Ball = World->SpawnActor<AJGBall>(BallClass ? BallClass.Get() : AJGBall::StaticClass(), SpawnLocation, FRotator::ZeroRotator, Params);

		if (Ball)
		{
			Ball->OnGroundContact.AddUObject(this, &AJGGameModeBase::HandleBallGroundContact);
			Ball->OnStopped.AddUObject(this, &AJGGameModeBase::HandleBallStopped);
		}
	}

	if (Ball && Court)
	{
		Ball->SetCourtFrame(Court->GetActorTransform());
	}

	if (AJGGameStateBase* JGGameState = GetJGGameState())
	{
		JGGameState->SetCourtAndBall(Court, Ball);
	}
}

void AJGGameModeBase::TryStartMatch()
{
	const AJGGameStateBase* JGGameState = GetJGGameState();
	if (bPracticeMode || !JGGameState || !HasActorBegunPlay())
	{
		return;
	}

	// 상대가 나가서 끝난 경기도 대기 단계처럼 새 플레이어와 함께 다시 고르고 준비한다
	if (JGGameState->IsCharacterSelectOpen()
		&& UJGMatchRules::CanStartMatch(CountTeamPlayers(), CountTeamPlayers(nullptr, true), GetRequiredPlayers()))
	{
		StartNewMatch();
	}
}

void AJGGameModeBase::StartNewMatch()
{
	AJGGameStateBase* JGGameState = GetJGGameState();
	if (!JGGameState)
	{
		return;
	}

	ClearPhaseTimer();

	FJGMatchSnapshot& Match = JGGameState->EditMatch();
	Match = FJGMatchSnapshot();

	// 첫 서브는 Host가 정해서 두 플레이어에게 복제한다
	const bool bHasA = HasPlayerOnTeam(EJGTeam::A);
	const bool bHasB = HasPlayerOnTeam(EJGTeam::B);
	if (bHasA && bHasB)
	{
		Match.ServingTeam = FMath::RandBool() ? EJGTeam::A : EJGTeam::B;
	}
	else
	{
		Match.ServingTeam = bHasB ? EJGTeam::B : EJGTeam::A;
	}

	for (APlayerState* PlayerState : JGGameState->PlayerArray)
	{
		if (AJGPlayerState* JGPlayerState = Cast<AJGPlayerState>(PlayerState))
		{
			JGPlayerState->SetWantsRematch(false);
		}
	}

	UE_LOG(LogDoJokgu, Log, TEXT("New match. First serve: %s"), *UEnum::GetValueAsString(Match.ServingTeam));

	EnterPreparingServe();
}

#pragma endregion

#pragma region 단계 전환과 득점

void AJGGameModeBase::EnterPreparingServe()
{
	AJGGameStateBase* JGGameState = GetJGGameState();
	if (!JGGameState || !Court || !Ball)
	{
		return;
	}

	const UJGBalanceData* Balance = UJGBalanceData::Get();

	// 서브할 팀에 사람이 없으면(혼자 경기 등) 상대 팀이 서브한다
	FJGMatchSnapshot& Match = JGGameState->EditMatch();
	if (!HasPlayerOnTeam(Match.ServingTeam))
	{
		const EJGTeam Other = UJGMatchRules::GetOpposingTeam(Match.ServingTeam);
		if (HasPlayerOnTeam(Other))
		{
			Match.ServingTeam = Other;
		}
	}

	++Match.RoundNumber;
	bPointResolvedThisRound = false;

	// 득점마다 위치, 공, 저장된 입력, 수비 상태를 한꺼번에 초기화한다
	ResetPlayersForServe();
	PlaceBallForServe();

	SetPhase(EJGMatchPhase::PreparingServe, Balance->ServePrepareTime);
	SetPhaseTimer(&AJGGameModeBase::EnterServing, Balance->ServePrepareTime);
}

void AJGGameModeBase::EnterServing()
{
	const float TimeLimit = UJGBalanceData::Get()->ServeTimeLimit;

	SetPhase(EJGMatchPhase::Serving, TimeLimit);
	SetPhaseTimer(&AJGGameModeBase::HandleServeTimeout, TimeLimit);
}

void AJGGameModeBase::HandleServeTimeout()
{
	if (const AJGGameStateBase* JGGameState = GetJGGameState())
	{
		AwardPoint(UJGMatchRules::GetOpposingTeam(JGGameState->GetServingTeam()), EJGPointReason::ServeTimeout);
	}
}

void AJGGameModeBase::FinishPoint()
{
	const AJGGameStateBase* JGGameState = GetJGGameState();
	if (!JGGameState)
	{
		return;
	}

	const UJGBalanceData* Balance = UJGBalanceData::Get();
	const FJGMatchSnapshot Match = JGGameState->GetMatch();
	const EJGTeam Winner = UJGMatchRules::GetMatchWinner(Match.ScoreA, Match.ScoreB, Balance->TargetScore, Balance->WinMargin);

	if (Winner != EJGTeam::None)
	{
		EndMatch(EJGMatchEndReason::ScoreReached, Winner);
	}
	else
	{
		EnterPreparingServe();
	}
}

void AJGGameModeBase::AwardPoint(EJGTeam ScoringTeam, EJGPointReason Reason)
{
	AJGGameStateBase* JGGameState = GetJGGameState();
	if (!JGGameState || bPointResolvedThisRound || ScoringTeam == EJGTeam::None)
	{
		return;
	}

	bPointResolvedThisRound = true;
	ClearPhaseTimer();

	// 공은 계속 굴러가게 두고 판정만 멈춘다
	if (Ball)
	{
		Ball->ClearRally();
	}

	FJGMatchSnapshot& Match = JGGameState->EditMatch();
	if (ScoringTeam == EJGTeam::A)
	{
		++Match.ScoreA;
	}
	else
	{
		++Match.ScoreB;
	}

	// 득점한 쪽이 다음 서브를 넣는다
	Match.LastScoringTeam = ScoringTeam;
	Match.LastPointReason = Reason;
	Match.ServingTeam = ScoringTeam;

	UE_LOG(LogDoJokgu, Log, TEXT("Point %s (%s). Score %d:%d"),
		*UEnum::GetValueAsString(ScoringTeam), *UEnum::GetValueAsString(Reason), Match.ScoreA, Match.ScoreB);

	const float PointEndTime = UJGBalanceData::Get()->PointEndTime;
	SetPhase(EJGMatchPhase::PointEnd, PointEndTime);
	SetPhaseTimer(&AJGGameModeBase::FinishPoint, PointEndTime);
}

void AJGGameModeBase::EndMatch(EJGMatchEndReason Reason, EJGTeam Winner)
{
	AJGGameStateBase* JGGameState = GetJGGameState();
	if (!JGGameState)
	{
		return;
	}

	ClearPhaseTimer();

	if (Ball)
	{
		Ball->ClearRally();
	}

	FJGMatchSnapshot& Match = JGGameState->EditMatch();
	Match.WinnerTeam = Winner;
	Match.EndReason = Reason;

	// 재경기 투표는 결과 화면에서 새로 받는다
	for (APlayerState* PlayerState : JGGameState->PlayerArray)
	{
		if (AJGPlayerState* JGPlayerState = Cast<AJGPlayerState>(PlayerState))
		{
			JGPlayerState->SetWantsRematch(false);
		}
	}

	// 남은 플레이어는 새 상대가 올 때까지 캐릭터를 다시 고를 수 있다
	if (Reason == EJGMatchEndReason::PlayerLeft)
	{
		ClearReadyFlags();
	}

	UE_LOG(LogDoJokgu, Log, TEXT("Match end: %s, winner %s"), *UEnum::GetValueAsString(Reason), *UEnum::GetValueAsString(Winner));

	SetPhase(EJGMatchPhase::MatchEnd, 0.0f);
}

#pragma endregion

#pragma region 단계 타이머

void AJGGameModeBase::SetPhase(EJGMatchPhase Phase, float Duration)
{
	AJGGameStateBase* JGGameState = GetJGGameState();
	if (!JGGameState)
	{
		return;
	}

	// 종료 시각을 서버 월드 시간으로 보내야 클라이언트가 같은 남은 시간을 계산한다
	FJGMatchSnapshot& Match = JGGameState->EditMatch();
	Match.Phase = Phase;
	Match.PhaseEndServerTime = Duration > 0.0f ? JGGameState->GetServerWorldTimeSeconds() + Duration : 0.0;

	JGGameState->CommitMatchChanges();
}

void AJGGameModeBase::SetPhaseTimer(void (AJGGameModeBase::*Callback)(), float Delay)
{
	// 대기 시간을 0으로 조정한 경우에도 단계가 멈추지 않도록 바로 넘어간다
	if (Delay <= 0.0f)
	{
		ClearPhaseTimer();
		(this->*Callback)();
		return;
	}

	GetWorldTimerManager().SetTimer(PhaseTimer, this, Callback, Delay, false);
}

void AJGGameModeBase::ClearPhaseTimer()
{
	GetWorldTimerManager().ClearTimer(PhaseTimer);
}

#pragma endregion

#pragma region 서브 배치

void AJGGameModeBase::ResetPlayersForServe()
{
	const AJGGameStateBase* JGGameState = GetJGGameState();
	if (!JGGameState || !Court)
	{
		return;
	}

	const EJGTeam ServingTeam = JGGameState->GetServingTeam();

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PlayerController = It->Get();
		const AJGPlayerState* PlayerState = PlayerController ? PlayerController->GetPlayerState<AJGPlayerState>() : nullptr;
		AJGCharacter* Character = PlayerController ? Cast<AJGCharacter>(PlayerController->GetPawn()) : nullptr;

		if (!PlayerState || !Character || PlayerState->GetTeam() == EJGTeam::None)
		{
			continue;
		}

		Character->ResetForNewPoint();

		const FTransform Start = Court->GetStartTransform(PlayerState->GetTeam(), PlayerState->GetTeam() == ServingTeam);
		Character->TeleportTo(Start.GetLocation(), Start.Rotator(), false, true);
		PlayerController->SetControlRotation(Start.Rotator());
	}
}

void AJGGameModeBase::PlaceBallForServe()
{
	const AJGGameStateBase* JGGameState = GetJGGameState();
	if (!JGGameState || !Court || !Ball)
	{
		return;
	}

	const UJGBalanceData* Balance = UJGBalanceData::Get();
	const EJGTeam ServingTeam = JGGameState->GetServingTeam();

	// 서브하는 캐릭터가 아직 없으면 시작 위치에서 발 높이를 추정한다
	FVector FootLocation = Court->GetStartTransform(ServingTeam, true).GetLocation() - FVector(0.0, 0.0, Balance->PlayerSpawnHeight);
	if (const AJGCharacter* Server = FindCharacterOnTeam(ServingTeam))
	{
		FootLocation = Server->GetFootLocation();
	}

	const FVector HoldLocation = FootLocation
		+ Court->GetAttackForward(ServingTeam) * Balance->ServeBallForwardOffset
		+ FVector(0.0, 0.0, Balance->ServeBallHeight);

	Ball->HoldAt(HoldLocation);
}

#pragma endregion

#pragma region 캐릭터 요청 처리

bool AJGGameModeBase::CanCharacterAttack(const AJGCharacter* Character, bool& bOutIsServe) const
{
	bOutIsServe = false;

	const AJGGameStateBase* JGGameState = GetJGGameState();
	if (!Character || !JGGameState)
	{
		return false;
	}

	if (bPracticeMode)
	{
		return true;
	}

	switch (JGGameState->GetPhase())
	{
	case EJGMatchPhase::Serving:
		// 서브도 일반 공격과 같은 탭/드래그 입력을 쓰고, 타격 범위 검사만 하지 않는다
		bOutIsServe = Character->GetTeam() == JGGameState->GetServingTeam();
		return bOutIsServe;

	case EJGMatchPhase::Rally:
		return true;

	default:
		return false;
	}
}

bool AJGGameModeBase::CanCharacterSlide(const AJGCharacter* Character) const
{
	const AJGGameStateBase* JGGameState = GetJGGameState();
	return Character && JGGameState && (bPracticeMode || JGGameState->GetPhase() == EJGMatchPhase::Rally);
}

void AJGGameModeBase::NotifyBallHit(AJGCharacter* Hitter, bool bWasServe)
{
	// 랠리 중 타격은 단계를 바꾸지 않는다. 서브가 들어가는 순간에만 Serving -> Rally로 넘긴다.
	const AJGGameStateBase* JGGameState = GetJGGameState();
	if (!bWasServe || bPracticeMode || !JGGameState || JGGameState->GetPhase() != EJGMatchPhase::Serving)
	{
		return;
	}

	ClearPhaseTimer();
	SetPhase(EJGMatchPhase::Rally, 0.0f);
}

void AJGGameModeBase::HandleRematchRequest(APlayerController* Requester)
{
	AJGGameStateBase* JGGameState = GetJGGameState();
	if (!Requester || !JGGameState || JGGameState->GetPhase() != EJGMatchPhase::MatchEnd)
	{
		return;
	}

	// 상대가 나가서 끝난 경기는 재경기가 아니라 새 플레이어를 기다린다
	if (JGGameState->GetMatch().EndReason != EJGMatchEndReason::ScoreReached)
	{
		return;
	}

	if (AJGPlayerState* PlayerState = Requester->GetPlayerState<AJGPlayerState>())
	{
		PlayerState->SetWantsRematch(true);
	}

	int32 Players = 0;
	int32 Votes = 0;

	for (APlayerState* PlayerState : JGGameState->PlayerArray)
	{
		const AJGPlayerState* JGPlayerState = Cast<AJGPlayerState>(PlayerState);
		if (JGPlayerState && JGPlayerState->GetTeam() != EJGTeam::None)
		{
			++Players;
			Votes += JGPlayerState->WantsRematch() ? 1 : 0;
		}
	}

	// 팀에 속한 모든 플레이어가 동의해야 새 경기를 시작한다
	if (Players >= GetRequiredPlayers() && Votes == Players)
	{
		StartNewMatch();
	}
}

void AJGGameModeBase::HandleCharacterSelect(APlayerController* Requester, FName CharacterId)
{
	AJGPlayerState* PlayerState = Requester ? Requester->GetPlayerState<AJGPlayerState>() : nullptr;
	UJGCharacterData* Data = UJGCharacterRoster::Get()->Find(CharacterId);

	if (!PlayerState || !Data)
	{
		UE_LOG(LogDoJokgu, Warning, TEXT("Character select ignored: %s is not in the roster"), *CharacterId.ToString());
		return;
	}

	// 경기 중 교체는 연습 모드에서만 받는다. 준비를 누른 뒤에는 상대가 본 선택을 바꾸지 않는다.
	const AJGGameStateBase* JGGameState = GetJGGameState();
	const bool bSelectOpen = JGGameState && JGGameState->IsCharacterSelectOpen();
	if (!bPracticeMode && (!bSelectOpen || PlayerState->IsReady()))
	{
		UE_LOG(LogDoJokgu, Verbose, TEXT("Character select ignored: not in the waiting phase or already ready"));
		return;
	}

	PlayerState->SetCharacterId(CharacterId);

	// 이미 스폰된 캐릭터에도 바로 적용한다
	if (AJGCharacter* Character = Cast<AJGCharacter>(Requester->GetPawn()))
	{
		Character->SetCharacterData(Data);
	}
}

void AJGGameModeBase::HandleReadyRequest(APlayerController* Requester, bool bReady)
{
	AJGPlayerState* PlayerState = Requester ? Requester->GetPlayerState<AJGPlayerState>() : nullptr;
	const AJGGameStateBase* JGGameState = GetJGGameState();

	if (bPracticeMode || !PlayerState || PlayerState->GetTeam() == EJGTeam::None || !JGGameState || !JGGameState->IsCharacterSelectOpen())
	{
		return;
	}

	PlayerState->SetReady(bReady);

	UE_LOG(LogDoJokgu, Log, TEXT("%s is %s (%s)"), *PlayerState->GetPlayerName(), bReady ? TEXT("ready") : TEXT("not ready"),
		*PlayerState->GetCharacterId().ToString());

	TryStartMatch();
}

#pragma endregion

#pragma region 공 판정

void AJGGameModeBase::HandleBallGroundContact(AJGBall* InBall, const FVector& Location)
{
	const AJGGameStateBase* JGGameState = GetJGGameState();
	if (!InBall || !Court || !JGGameState)
	{
		return;
	}

	// 이미 득점이 결정된 라운드의 접촉은 무시한다
	if (!bPracticeMode && (JGGameState->GetPhase() != EJGMatchPhase::Rally || bPointResolvedThisRound))
	{
		return;
	}

	const EJGCourtZone Zone = Court->ClassifyWorldLocation(Location);
	const FJGContactResult Result = UJGMatchRules::EvaluateGroundContact(InBall->GetMutableRallyState(), Zone);

	UE_LOG(LogDoJokgu, Verbose, TEXT("Ground contact in %s, point scored: %d"), *UEnum::GetValueAsString(Zone), Result.bPointScored);

	ApplyContactResult(Result);
}

void AJGGameModeBase::HandleBallStopped(AJGBall* InBall, const FVector& Location)
{
	const AJGGameStateBase* JGGameState = GetJGGameState();
	if (!InBall || !JGGameState)
	{
		return;
	}

	if (!bPracticeMode && (JGGameState->GetPhase() != EJGMatchPhase::Rally || bPointResolvedThisRound))
	{
		return;
	}

	ApplyContactResult(UJGMatchRules::EvaluateBallStopped(InBall->GetRallyState()));
}

void AJGGameModeBase::ApplyContactResult(const FJGContactResult& Result)
{
	if (!Result.bPointScored)
	{
		return;
	}

	// 득점을 결정한 샷의 네트 통과 기록. 착지 판정과 비교해 네트 관련 판정 문제를 진단하는 데 쓴다.
	FString NetInfo = TEXT("net not crossed");
	if (Ball && Ball->GetRallyState().bCrossedNet)
	{
		const FJGRallyState& Rally = Ball->GetRallyState();
		NetInfo = FString::Printf(TEXT("net clearance %.0f cm, lateral %.0f cm"), Rally.NetCrossingClearance, Rally.NetCrossingLateral);
	}

	UE_LOG(LogDoJokgu, Log, TEXT("Point %s (%s), shot %d, %s"),
		*UEnum::GetDisplayValueAsText(Result.ScoringTeam).ToString(), *UEnum::GetDisplayValueAsText(Result.Reason).ToString(),
		Ball ? Ball->GetRallyState().ShotId : 0, *NetInfo);

	// 연습 모드는 점수를 올리지 않고 판정 결과만 화면에 띄운다
	if (bPracticeMode)
	{
		if (Ball)
		{
			Ball->ClearRally();
		}

		const FString Message = FString::Printf(TEXT("[Practice] Point %s - %s (%s)"),
			*UEnum::GetDisplayValueAsText(Result.ScoringTeam).ToString(), *UEnum::GetDisplayValueAsText(Result.Reason).ToString(), *NetInfo);

		UE_LOG(LogDoJokgu, Log, TEXT("%s"), *Message);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Yellow, Message);
		}

		return;
	}

	AwardPoint(Result.ScoringTeam, Result.Reason);
}

#pragma endregion

#pragma region 조회 도우미

EJGTeam AJGGameModeBase::FindFreeTeam() const
{
	const AJGGameStateBase* JGGameState = GetJGGameState();
	if (!JGGameState)
	{
		return EJGTeam::A;
	}

	bool bATaken = false;
	bool bBTaken = false;

	for (const APlayerState* PlayerState : JGGameState->PlayerArray)
	{
		if (const AJGPlayerState* JGPlayerState = Cast<AJGPlayerState>(PlayerState))
		{
			bATaken |= JGPlayerState->GetTeam() == EJGTeam::A;
			bBTaken |= JGPlayerState->GetTeam() == EJGTeam::B;
		}
	}

	if (!bATaken)
	{
		return EJGTeam::A;
	}

	return bBTaken ? EJGTeam::None : EJGTeam::B;
}

int32 AJGGameModeBase::CountTeamPlayers(const AController* Ignore, bool bReadyOnly) const
{
	const AJGGameStateBase* JGGameState = GetJGGameState();
	if (!JGGameState)
	{
		return 0;
	}

	const APlayerState* IgnoredState = Ignore ? Ignore->PlayerState.Get() : nullptr;
	int32 Count = 0;

	for (const APlayerState* PlayerState : JGGameState->PlayerArray)
	{
		const AJGPlayerState* JGPlayerState = Cast<AJGPlayerState>(PlayerState);
		if (JGPlayerState && JGPlayerState != IgnoredState && JGPlayerState->GetTeam() != EJGTeam::None
			&& (!bReadyOnly || JGPlayerState->IsReady()))
		{
			++Count;
		}
	}

	return Count;
}

void AJGGameModeBase::ClearReadyFlags()
{
	if (const AJGGameStateBase* JGGameState = GetJGGameState())
	{
		for (APlayerState* PlayerState : JGGameState->PlayerArray)
		{
			if (AJGPlayerState* JGPlayerState = Cast<AJGPlayerState>(PlayerState))
			{
				JGPlayerState->SetReady(false);
			}
		}
	}
}

bool AJGGameModeBase::HasPlayerOnTeam(EJGTeam Team) const
{
	const AJGGameStateBase* JGGameState = GetJGGameState();
	if (!JGGameState || Team == EJGTeam::None)
	{
		return false;
	}

	for (const APlayerState* PlayerState : JGGameState->PlayerArray)
	{
		const AJGPlayerState* JGPlayerState = Cast<AJGPlayerState>(PlayerState);
		if (JGPlayerState && JGPlayerState->GetTeam() == Team)
		{
			return true;
		}
	}

	return false;
}

AJGCharacter* AJGGameModeBase::FindCharacterOnTeam(EJGTeam Team) const
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		AJGCharacter* Character = PlayerController ? Cast<AJGCharacter>(PlayerController->GetPawn()) : nullptr;

		if (Character && Character->GetTeam() == Team)
		{
			return Character;
		}
	}

	return nullptr;
}

UJGCharacterData* AJGGameModeBase::GetStartingCharacterData(EJGTeam Team) const
{
	// 로스터가 비어 있거나 해당 칸이 비었으면 기본 선수로 시작한다
	return UJGCharacterRoster::Get()->GetAtOrDefault(Team == EJGTeam::B ? 1 : 0);
}

int32 AJGGameModeBase::GetRequiredPlayers() const
{
	return bAllowSoloMatch ? 1 : MaxMatchPlayers;
}

AJGGameStateBase* AJGGameModeBase::GetJGGameState() const
{
	return GetGameState<AJGGameStateBase>();
}

#pragma endregion
