// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ArcaneSessionSubsystem.generated.h"

USTRUCT(BlueprintType)
struct FArcaneSessionInfo
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly, Category = "Arcane|Session")
	FString SessionName;
	
	UPROPERTY(BlueprintReadOnly, Category = "Arcane|Session")
	int32 Ping = 0;
	
	UPROPERTY(BlueprintReadOnly, Category = "Arcane|Session")
	int32 CurrentPlayers = 0;
	
	UPROPERTY(BlueprintReadOnly, Category = "Arcane|Session")
	int32 MaxPlayers = 0;
	
	UPROPERTY(BlueprintReadOnly, Category = "Arcane|Session")
	int32 SearchIndex = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnArcaneSessionFound, const TArray<FArcaneSessionInfo>&, Sessions);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnArcaneSessionCreated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnArcaneSessionJoined);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnArcaneSessionDestroyed);


/**
 * 
 */
UCLASS()
class ARCANE_ARENA_API UArcaneSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	
	virtual void Deinitialize() override;
	
	UFUNCTION(BlueprintCallable, Category = "Arcane|Session")
	void CreateArcaneSession(int32 NumPublicConnections = 6,
		const FString& MapName = TEXT("Lvl_Combat"),
		const FString& DisplayName = TEXT("Arcane Arena"));
	
	UFUNCTION(BlueprintCallable, Category = "Arcane|Session")
	void FindArcaneSessions();
	
	UFUNCTION(BlueprintCallable, Category = "Arcane|Session")
	void JoinArcaneSession(int32 SearchIndex);
	
	UFUNCTION(BlueprintCallable, Category = "Arcane|Session")
	void DestroyArcaneSession();
	
	// 房主侧唯一入口：把房间的对外容量/人数对齐到真实状态（幂等，非主机调用为 no-op）
	// 容量真相在 GameMode（NumTeams × MaxPlayersPerTeam），会话只是它的对外投影
	UFUNCTION(BlueprintCallable, Category = "Arcane|Session")
	void SyncArcaneSessionRoster(int32 TotalCapacity, int32 NumRealPlayers);
	
	UFUNCTION(BlueprintCallable, Category = "Arcane|Session")
	void ReturnToLobby(const FString& MenuMapName = TEXT("Lvl_MainMenu"));
	
	UPROPERTY(BlueprintAssignable, Category = "Arcane|Session")
	FOnArcaneSessionFound OnArcaneSessionFound;
	
	UPROPERTY(BlueprintAssignable, Category = "Arcane|Session")
	FOnArcaneSessionCreated OnArcaneSessionCreated;
	
	UPROPERTY(BlueprintAssignable, Category = "Arcane|Session")
	FOnArcaneSessionJoined OnArcaneSessionJoined;
	
	UPROPERTY(BlueprintAssignable, Category = "Arcane|Session")
	FOnArcaneSessionDestroyed OnArcaneSessionDestroyed;
	
	FDelegateHandle CreateHandle;
	FDelegateHandle FindHandle;
	FDelegateHandle JoinHandle;
	FDelegateHandle DestroyHandle;
	
protected:
	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	
	void OnFindSessionsComplete(bool bWasSuccessful);
	
	void OnJoinSessionsComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	
	void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	
	IOnlineSessionPtr GetSessionInterface() const;
	
	UWorld* GetSafeWorld() const;
	
	TSharedPtr<FOnlineSessionSearch> SessionSearch;
	FName CurrentSessionName = FName(TEXT("ArcaneSession"));
	FString PendingTravelMap = TEXT("Lvl_Combat");
	
	
};
