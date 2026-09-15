#include "Jokgu/Core/JGGameModeBase.h"
#include "Jokgu/Core/JGGameStateBase.h"
#include "Jokgu/Core/JGMatchRules.h"
#include "Jokgu/Core/JGPlayerState.h"
#include "Jokgu/Data/JGBalanceData.h"
#include "Jokgu/Data/JGCharacterData.h"
#include "Jokgu/Gameplay/JGBall.h"
#include "Jokgu/Gameplay/JGCourt.h"
#include "Jokgu/Player/JGCharacter.h"
#include "Jokgu/Player/JGPlayerController.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "DoJokgu.h"

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

	if (AJGPlayerState* PlayerState = NewPlayer ? NewPlayer->GetPlayerState<AJGPlayerState>() : nullptr)
	{
		if (PlayerState->GetTeam() == EJGTeam::None)
		{
			PlayerState->SetTeam(FindFreeTeam());
		}

		if (PlayerState->GetCharacterId().IsNone() && DefaultCharacterData)
		{
			PlayerState->SetCharacterId(DefaultCharacterData->CharacterId);
		}
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

	if (!Court || !PlayerState || PlayerState->GetTeam() == EJGTeam::None || !JGGameState)
	{
		Super::RestartPlayer(NewPlayer);
		return;
	}

	const bool bIsServing = PlayerState->GetTeam() == JGGameState->GetServingTeam();
	RestartPlayerAtTransform(NewPlayer, Court->GetStartTransform(PlayerState->GetTeam(), bIsServing));

	if (AJGCharacter* Character = Cast<AJGCharacter>(NewPlayer->GetPawn()))
	{
		Character->SetCharacterData(FindCharacterData(PlayerState->GetCharacterId()));
	}
}

void AJGGameModeBase::Logout(AController* Exiting)
{
	AJGPlayerState* PlayerState = Exiting ? Exiting->GetPlayerState<AJGPlayerState>() : nullptr;
	const bool bWasOnTeam = PlayerState && PlayerState->GetTeam() != EJGTeam::None;

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

	// first version: leaving ends the match, the remaining player sees the reason
	if (CountTeamPlayers(Exiting) < GetRequiredPlayers())
	{
		EndMatch(EJGMatchEndReason::PlayerLeft, EJGTeam::None);
	}
}

void AJGGameModeBase::StartPlay()
{
	EnsureCourtAndBall();

	Super::StartPlay();

	if (bPracticeMode)
	{
		SetPhase(EJGMatchPhase::Rally, 0.0f);
		return;
	}

	TryStartMatch();
}

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

	const FJGMatchSnapshot Match = JGGameState->GetMatch();
	const bool bCanStart = Match.Phase == EJGMatchPhase::WaitingPlayers
		|| (Match.Phase == EJGMatchPhase::MatchEnd && Match.EndReason == EJGMatchEndReason::PlayerLeft);

	if (bCanStart && CountTeamPlayers() >= GetRequiredPlayers())
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

	// first serve is decided by the host and replicated to both players
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

void AJGGameModeBase::EnterPreparingServe()
{
	AJGGameStateBase* JGGameState = GetJGGameState();
	if (!JGGameState || !Court || !Ball)
	{
		return;
	}

	const UJGBalanceData* Balance = UJGBalanceData::Get();

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

	// every point resets positions, the ball, buffered inputs and defensive state together
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

	// serve goes to the player who scored
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

	for (APlayerState* PlayerState : JGGameState->PlayerArray)
	{
		if (AJGPlayerState* JGPlayerState = Cast<AJGPlayerState>(PlayerState))
		{
			JGPlayerState->SetWantsRematch(false);
		}
	}

	UE_LOG(LogDoJokgu, Log, TEXT("Match end: %s, winner %s"), *UEnum::GetValueAsString(Reason), *UEnum::GetValueAsString(Winner));

	SetPhase(EJGMatchPhase::MatchEnd, 0.0f);
}

void AJGGameModeBase::SetPhase(EJGMatchPhase Phase, float Duration)
{
	AJGGameStateBase* JGGameState = GetJGGameState();
	if (!JGGameState)
	{
		return;
	}

	FJGMatchSnapshot& Match = JGGameState->EditMatch();
	Match.Phase = Phase;
	Match.PhaseEndServerTime = Duration > 0.0f ? JGGameState->GetServerWorldTimeSeconds() + Duration : 0.0;

	JGGameState->CommitMatchChanges();
}

void AJGGameModeBase::SetPhaseTimer(void (AJGGameModeBase::*Callback)(), float Delay)
{
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
		// the serve uses the same tap/drag input as any attack, without the reach check
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

	if (Players >= GetRequiredPlayers() && Votes == Players)
	{
		StartNewMatch();
	}
}

void AJGGameModeBase::HandleBallGroundContact(AJGBall* InBall, const FVector& Location)
{
	const AJGGameStateBase* JGGameState = GetJGGameState();
	if (!InBall || !Court || !JGGameState)
	{
		return;
	}

	// ignore contacts from a round whose point is already decided
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

	if (bPracticeMode)
	{
		if (Ball)
		{
			Ball->ClearRally();
		}

		const FString Message = FString::Printf(TEXT("[Practice] Point %s - %s"),
			*UEnum::GetDisplayValueAsText(Result.ScoringTeam).ToString(), *UEnum::GetDisplayValueAsText(Result.Reason).ToString());

		UE_LOG(LogDoJokgu, Log, TEXT("%s"), *Message);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Yellow, Message);
		}

		return;
	}

	AwardPoint(Result.ScoringTeam, Result.Reason);
}

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

int32 AJGGameModeBase::CountTeamPlayers(const AController* Ignore) const
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
		if (JGPlayerState && JGPlayerState != IgnoredState && JGPlayerState->GetTeam() != EJGTeam::None)
		{
			++Count;
		}
	}

	return Count;
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

UJGCharacterData* AJGGameModeBase::FindCharacterData(FName CharacterId) const
{
	for (UJGCharacterData* Data : CharacterRoster)
	{
		if (Data && Data->CharacterId == CharacterId)
		{
			return Data;
		}
	}

	return DefaultCharacterData;
}

int32 AJGGameModeBase::GetRequiredPlayers() const
{
	return bAllowSoloMatch ? 1 : MaxMatchPlayers;
}

AJGGameStateBase* AJGGameModeBase::GetJGGameState() const
{
	return GetGameState<AJGGameStateBase>();
}
