#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/EngineBaseTypes.h"
#include "JGSessionSubsystem.generated.h"

class UNetDriver;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FJGConnectionFailedSignature, const FString&, Reason);

/**
 *  Room lifetime: host, join, leave and connection failures. Lives with the GameInstance, never stores match scores.
 *  First prototype: listen server + direct address join on the same network.
 *  Next step: replace Host/Join internals with EOS sessions (login, room lookup, P2P NetDriver) behind the same API.
 */
UCLASS()
class UJGSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Opens the match map as a listen server. The host is a player and the authority. */
	UFUNCTION(BlueprintCallable, Category="Jokgu|Session")
	void HostMatch(const FString& MapPath);

	/** Joins a host by IP[:port] on the local network */
	UFUNCTION(BlueprintCallable, Category="Jokgu|Session")
	void JoinByAddress(const FString& Address);

	/** Common exit path for leave, failures and host shutdown */
	UFUNCTION(BlueprintCallable, Category="Jokgu|Session")
	void LeaveToMenu(const FString& MenuMapPath);

	UFUNCTION(BlueprintPure, Category="Jokgu|Session")
	FString GetLastFailureReason() const { return LastFailureReason; }

	UPROPERTY(BlueprintAssignable, Category="Jokgu|Session")
	FJGConnectionFailedSignature OnConnectionFailed;

protected:

	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);
	void ReportFailure(const FString& Reason);

	FString LastFailureReason;
	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;
};
