// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneSessionSubsystem.h"

#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Online/OnlineSessionNames.h"

void UArcaneSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UArcaneSessionSubsystem::Deinitialize()
{
	if (IOnlineSessionPtr S = GetSessionInterface())
	{
		S->OnCreateSessionCompleteDelegates.Remove(CreateHandle);
		S->OnFindSessionsCompleteDelegates.Remove(FindHandle);
		S->OnJoinSessionCompleteDelegates.Remove(JoinHandle);
		S->OnDestroySessionCompleteDelegates.Remove(DestroyHandle);
	}
	
	Super::Deinitialize();
}

void UArcaneSessionSubsystem::CreateArcaneSession(int32 NumPublicConnections, const FString& MapName,
	const FString& DisplayName)
{
	IOnlineSessionPtr S = GetSessionInterface();
	if (S == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ArcaneSession] 无 OnlineSubsystem"));
		return;
	}
	
	PendingTravelMap = MapName;
	if (S->GetNamedSession(CurrentSessionName))
	{
		S->DestroySession(CurrentSessionName); // 如果已经有回话了，需要先销毁
	}
	
	FOnlineSessionSettings Settings;
	Settings.bIsLANMatch = true;
	Settings.bUsesPresence = false;
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bAllowJoinViaPresence = false;
	Settings.bIsDedicated = false;
	Settings.NumPublicConnections = NumPublicConnections;
	Settings.NumPrivateConnections = 0;
	Settings.Set(SETTING_MAPNAME, MapName, EOnlineDataAdvertisementType::ViaOnlineService);
	Settings.Set(SETTING_SESSION_TEMPLATE_NAME, DisplayName, EOnlineDataAdvertisementType::ViaOnlineService);
	
	CreateHandle = S->OnCreateSessionCompleteDelegates.AddUObject(this, &UArcaneSessionSubsystem::OnCreateSessionComplete);
	S->CreateSession(0, CurrentSessionName, Settings);
}

void UArcaneSessionSubsystem::FindArcaneSessions()
{
	IOnlineSessionPtr S = GetSessionInterface();
	if (S == nullptr)
	{
		return;
	}
	
	SessionSearch = MakeShared<FOnlineSessionSearch>();
	SessionSearch->bIsLanQuery = true;
	SessionSearch->MaxSearchResults = 10;
	// SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	FindHandle = S->OnFindSessionsCompleteDelegates.AddUObject(this, &UArcaneSessionSubsystem::OnFindSessionsComplete);
	
	S->FindSessions(0, SessionSearch.ToSharedRef());
}

void UArcaneSessionSubsystem::JoinArcaneSession(int32 SearchIndex)
{
	IOnlineSessionPtr S = GetSessionInterface();
	if (S == nullptr || !SessionSearch.IsValid())
	{
		return;
	}
	
	if (!SessionSearch->SearchResults.IsValidIndex(SearchIndex))
	{
		return;
	}
	
	JoinHandle = S->OnJoinSessionCompleteDelegates.AddUObject(this, &UArcaneSessionSubsystem::OnJoinSessionsComplete);
	S->JoinSession(0, CurrentSessionName, SessionSearch->SearchResults[SearchIndex]);
}

void UArcaneSessionSubsystem::DestroyArcaneSession()
{
	if (IOnlineSessionPtr S = GetSessionInterface())
	{
		DestroyHandle = S->OnDestroySessionCompleteDelegates.AddUObject(this, &UArcaneSessionSubsystem::OnDestroySessionComplete);
		S->DestroySession(CurrentSessionName);
	}
}

void UArcaneSessionSubsystem::SyncArcaneSessionRoster(int32 TotalCapacity, int32 NumRealPlayers)
{
	IOnlineSessionPtr S = GetSessionInterface();
	if (!S.IsValid())
	{
		return;
	}
	
	FNamedOnlineSession* Session = S->GetNamedSession(CurrentSessionName);
	// 只有主机那份会话会被 LAN 广播出去，改客机的副本没有任何意义
	if (Session == nullptr || Session->bHosting == false)
	{
		return;
	}
	
	const int32 NewCapacity = FMath::Max(1, TotalCapacity);
	if (Session->SessionSettings.NumPublicConnections != NewCapacity)
	{
		// 必须拷一份再更新：Null 的 UpdateSession 内部是 SessionSettings = Updated，
		// 直接把活对象自己的成员当入参会走自赋值
		FOnlineSessionSettings Updated = Session->SessionSettings;
		Updated.NumPublicConnections = NewCapacity;
		S->UpdateSession(CurrentSessionName, Updated, true);
		Session = S->GetNamedSession(CurrentSessionName);
		if (Session == nullptr)
		{
			return;
		}
	}
	
	Session->NumOpenPublicConnections = FMath::Clamp(NewCapacity - FMath::Max(0, NumRealPlayers), 0, NewCapacity);
}

void UArcaneSessionSubsystem::ReturnToLobby(const FString& MenuMapName)
{
	DestroyArcaneSession();
	
	if (UWorld* W = GetSafeWorld())
	{
		if (W->GetNetMode() == NM_ListenServer || W->GetNetMode() == NM_DedicatedServer)
		{
			W->ServerTravel(MenuMapName);
		}
		else if (APlayerController* PC = W->GetFirstPlayerController())
		{
			PC->ClientTravel(MenuMapName, TRAVEL_Absolute);
		}
	}
}

void UArcaneSessionSubsystem::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (IOnlineSessionPtr S = GetSessionInterface())
	{
		S->OnCreateSessionCompleteDelegates.Remove(CreateHandle);
	}
	
	if (bWasSuccessful == false)
	{
		UE_LOG(LogTemp, Error, TEXT("[ArcaneSession] 会话创建失败"));
		
		return;
	}
	
	OnArcaneSessionCreated.Broadcast();
	
	if (UWorld* W = GetSafeWorld())
	{
		if (W->GetNetMode() != NM_Client)
		{
			W->ServerTravel(PendingTravelMap + TEXT("?listen"));
		}
	}
}

void UArcaneSessionSubsystem::OnFindSessionsComplete(bool bWasSuccessful)
{
	if (IOnlineSessionPtr S = GetSessionInterface())
	{
		S->OnFindSessionsCompleteDelegates.Remove(FindHandle);
	}
	
	TArray<FArcaneSessionInfo> Results;
	
	if (bWasSuccessful && SessionSearch.IsValid())
	{
		for (int32 i = 0; i < SessionSearch->SearchResults.Num(); i++)
		{
			const FOnlineSessionSearchResult& Result = SessionSearch->SearchResults[i];
			FArcaneSessionInfo Info;
			Info.SearchIndex = i;
			Result.Session.SessionSettings.Get(SETTING_SESSION_TEMPLATE_NAME, Info.SessionName);
			
			if (Info.SessionName.IsEmpty())
			{
				Info.SessionName = Result.Session.OwningUserName;
			}
			
			Info.MaxPlayers = Result.Session.SessionSettings.NumPublicConnections;
			Info.CurrentPlayers = Info.MaxPlayers - Result.Session.NumOpenPublicConnections;
			Info.Ping = Result.PingInMs;
			
			Results.Add(Info);
		}
	}
	
	OnArcaneSessionFound.Broadcast(Results);
}

void UArcaneSessionSubsystem::OnJoinSessionsComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	IOnlineSessionPtr S = GetSessionInterface();
	
	if (S != nullptr)
	{
		S->OnJoinSessionCompleteDelegates.Remove(JoinHandle);
	}
	
	if (Result != EOnJoinSessionCompleteResult::Success)
	{
		UE_LOG(LogTemp, Error, TEXT("[ArcaneSession] 加入会话失败: %d"), (int32)Result);
		return;
	}
	
	FString ConnectString;
	if (S->GetResolvedConnectString(SessionName, ConnectString))
	{
		if (UWorld* W = GetSafeWorld())
		{
			if (APlayerController* PC = W->GetFirstPlayerController())
			{
				PC->ClientTravel(ConnectString, TRAVEL_Absolute);
			}
		}
	}
	
	OnArcaneSessionJoined.Broadcast();
}

void UArcaneSessionSubsystem::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (IOnlineSessionPtr S = GetSessionInterface())
	{
		S->OnDestroySessionCompleteDelegates.Remove(DestroyHandle);
	}
	
	OnArcaneSessionDestroyed.Broadcast();
}

IOnlineSessionPtr UArcaneSessionSubsystem::GetSessionInterface() const
{
	if (IOnlineSubsystem* Sub = Online::GetSubsystem(GetWorld()))
	{
		return Sub->GetSessionInterface();
	}
	
	return nullptr;
}

UWorld* UArcaneSessionSubsystem::GetSafeWorld() const
{
	if (UGameInstance* GI = GetGameInstance())
	{
		return GI->GetWorld();
	}
	
	return nullptr;
}

