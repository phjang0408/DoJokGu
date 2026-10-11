#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/EngineBaseTypes.h"
#include "JGSessionSubsystem.generated.h"

class UNetDriver;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FJGConnectionFailedSignature, const FString&, Reason);

/**
 *  방의 수명: 호스트, 참가, 나가기, 접속 실패. GameInstance와 함께 살며 경기 점수는 저장하지 않는다.
 *  첫 프로토타입: Listen Server + 같은 네트워크에서 주소로 직접 참가.
 *  다음 단계: 같은 API 뒤에서 Host/Join 내부를 EOS 세션(로그인, 방 찾기, P2P NetDriver)으로 바꾼다.
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

	/** 경기 맵을 Listen Server로 연다. Host는 플레이어이자 권한자다. */
	UFUNCTION(BlueprintCallable, Category="Jokgu|Session")
	void HostMatch(const FString& MapPath);

	/** 로컬 네트워크의 Host에 IP[:port]로 참가한다 */
	UFUNCTION(BlueprintCallable, Category="Jokgu|Session")
	void JoinByAddress(const FString& Address);

	/** 나가기, 실패, Host 종료가 함께 쓰는 공통 퇴장 경로 */
	UFUNCTION(BlueprintCallable, Category="Jokgu|Session")
	void LeaveToMenu(const FString& MenuMapPath);

#pragma endregion

#pragma region 실패 알림

	/** 마지막 실패 사유. 새로 호스트하거나 참가하면 비운다. */
	UFUNCTION(BlueprintPure, Category="Jokgu|Session")
	FString GetLastFailureReason() const { return LastFailureReason; }

	UPROPERTY(BlueprintAssignable, Category="Jokgu|Session")
	FJGConnectionFailedSignature OnConnectionFailed;

#pragma endregion

protected:

	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);

	/** 사유를 저장하고 로그를 남긴 뒤 OnConnectionFailed로 알린다 */
	void ReportFailure(const FString& Reason);

	FString LastFailureReason;
	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;
};
