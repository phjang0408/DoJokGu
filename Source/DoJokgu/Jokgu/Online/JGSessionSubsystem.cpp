#include "Jokgu/Online/JGSessionSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "DoJokgu.h"

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

	Super::Deinitialize();
}

void UJGSessionSubsystem::HostMatch(const FString& MapPath)
{
	LastFailureReason.Reset();
	UE_LOG(LogDoJokgu, Log, TEXT("Hosting listen server on %s"), *MapPath);
	UGameplayStatics::OpenLevel(this, FName(*MapPath), true, TEXT("listen"));
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

void UJGSessionSubsystem::LeaveToMenu(const FString& MenuMapPath)
{
	UE_LOG(LogDoJokgu, Log, TEXT("Leaving match to %s"), *MenuMapPath);

	// opening a level without ?listen closes the listen server or the client connection
	UGameplayStatics::OpenLevel(this, FName(*MenuMapPath), true);
}

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
