// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneRosterComponent.h"

#include "ArcaneGameMode.h"
#include "Arcane_Arena.h"
#include "EngineUtils.h"
#include "AI/ArcaneAIController.h"
#include "Character/ArcaneBotCharacter.h"
#include "Character/ArcaneCharacter.h"
#include "Engine/PlayerStartPIE.h"
#include "Online/ArcaneSessionSubsystem.h"
#include "Player/ArcanePlayerState.h"


// Sets default values for this component's properties
UArcaneRosterComponent::UArcaneRosterComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}

int32 UArcaneRosterComponent::PickTeamForNewPlayer() const
{
	const AArcaneGameMode* GM = GetOwnerGM();
	if (GM == nullptr)
	{
		return INDEX_NONE;
	}
	
	const int32 TeamCount = FMath::Max(1, GM->GetNumTeams());
	TArray<int32> TeamSizes;
	TeamSizes.Init(0, TeamCount);
	
	if (const AArcaneGameState* GS = GetWorld() ? GetWorld()->GetGameState<AArcaneGameState>() : nullptr)
	{
		for (APlayerState* PS : GS->PlayerArray)
		{
			const AArcanePlayerState* ArcPS = Cast<AArcanePlayerState>(PS);
			if (ArcPS && ArcPS->TeamID >= 0 && ArcPS->TeamID < TeamCount)
			{
				TeamSizes[ArcPS->TeamID]++;
			}
		}
	}
	
	int32 BestTeam = INDEX_NONE;
	
	for (int32 Team = 0; Team < TeamCount; ++Team)
	{
		if (TeamSizes[Team] >= FMath::Max(1, GM->GetMaxPlayersPerTeam()))
		{
			continue;
		}
		if (BestTeam == INDEX_NONE || TeamSizes[Team] < TeamSizes[BestTeam])
		{
			BestTeam = Team;
		}
	}
 
	return BestTeam;
}

int32 UArcaneRosterComponent::EvictBotForRealPlayer()
{
	const AArcaneGameMode* GM = GetOwnerGM();
	if (GM == nullptr)
	{
		return 0;
	}
	
	const int32 TeamCount = FMath::Max(1, GM->GetNumTeams());
	int32 BestTeam = INDEX_NONE;
	int32 BestBots = 0;
	
	for (int32 Team = 0; Team < TeamCount; Team++)
	{
		const int32 Bots = CountTeamBots(Team);
		if (Bots > BestBots)
		{
			BestBots = Bots;
			BestTeam = Team;
		}
	}
	
	if (BestTeam != INDEX_NONE && RemoveOneBotFromTeam(BestTeam))
	{
		UE_LOG(LogArcane, Log, TEXT("EvictBotForRealPlayer: 真人进场，顶掉 Team%d 的 1 个 Bot 让位"), BestTeam);
		return BestTeam;
	}
 
	int32 Smallest = 0;
	for (int32 Team = 1; Team < TeamCount; Team++)
	{
		if (CountTeamMembers(Team) < CountTeamMembers(Smallest))
		{
			Smallest = Team;
		}
	}
 
	UE_LOG(LogArcane, Warning, TEXT("EvictBotForRealPlayer: 无 Bot 可顶，真人临时超编进 Team%d"), Smallest);
	return Smallest;
}

bool UArcaneRosterComponent::SetTeam(AArcanePlayerState* PS, int32 NewTeamID)
{
	if (PS == nullptr)
	{
		return false;
	}
 
	const AArcaneGameMode* GM = GetOwnerGM();
	const AArcaneGameState* GS = GetWorld() ? GetWorld()->GetGameState<AArcaneGameState>() : nullptr;
	if (GM == nullptr || GS == nullptr || GS->MatchPhase != EArcaneMatchPhase::Warmup)
	{
		return false;
	}
 
	if (NewTeamID < 0 || NewTeamID >= FMath::Max(1, GM->GetNumTeams()) || NewTeamID == PS->TeamID)
	{
		return false;
	}
 
	if (CountTeamMembers(NewTeamID) >= GM->GetMaxPlayersPerTeam())
	{
		UE_LOG(LogArcane, Log, TEXT("HandleTeamRequest: %s 想进 Team%d，但已满(%d/%d)"),
			*PS->GetPlayerName(), NewTeamID, CountTeamMembers(NewTeamID), GM->GetMaxPlayersPerTeam());
		return false;
	}
 
	PS->TeamID = NewTeamID;
	return true;
}

bool UArcaneRosterComponent::RemoveOneBotFromTeam(int32 TeamID)
{
	const AArcaneGameState* GS = GetWorld() ? GetWorld()->GetGameState<AArcaneGameState>() : nullptr;
	const AArcaneGameMode* GM = GetOwnerGM();
	if (GS == nullptr || GM == nullptr)
	{
		return false;
	}
 
	for (int32 i = GS->PlayerArray.Num() - 1; i >= 0; i--)
	{
		const AArcanePlayerState* ArcPS = Cast<AArcanePlayerState>(GS->PlayerArray[i]);
		if (ArcPS != nullptr && ArcPS->bIsBot && ArcPS->TeamID == TeamID)
		{
			if (AArcaneAIController* Bot = Cast<AArcaneAIController>(ArcPS->GetOwner()))
			{
				Bot->DestroyBotCompletely();
				return true;
			}
		}
	}
 
	return false;
}

int32 UArcaneRosterComponent::CountTeamMembers(int32 TeamID) const
{
	int32 Count = 0;
 
	if (const AArcaneGameState* GS = GetWorld() ? GetWorld()->GetGameState<AArcaneGameState>() : nullptr)
	{
		for (APlayerState* PS : GS->PlayerArray)
		{
			if (const AArcanePlayerState* ArcPS = Cast<AArcanePlayerState>(PS))
			{
				if (ArcPS->TeamID == TeamID)
				{
					Count++;
				}
			}
		}
	}
 
	return Count;
}

int32 UArcaneRosterComponent::CountTeamBots(int32 TeamID) const
{
	int32 Count = 0;
 
	if (const AArcaneGameState* GS = GetWorld() ? GetWorld()->GetGameState<AArcaneGameState>() : nullptr)
	{
		for (APlayerState* PS : GS->PlayerArray)
		{
			if (const AArcanePlayerState* ArcPS = Cast<AArcanePlayerState>(PS))
			{
				if (ArcPS->bIsBot && ArcPS->TeamID == TeamID)
				{
					Count++;
				}
			}
		}
	}
 
	return Count;
}

int32 UArcaneRosterComponent::CountConnectedRealPlayers(APlayerState* Exclude) const
{
	int32 Count = 0;
 
	if (const AArcaneGameState* GS = GetWorld() ? GetWorld()->GetGameState<AArcaneGameState>() : nullptr)
	{
		for (APlayerState* PS : GS->PlayerArray)
		{
			if (PS == nullptr || PS == Exclude || PS->IsInactive())
			{
				continue;
			}
 
			const AArcanePlayerState* ArcPS = Cast<AArcanePlayerState>(PS);
			if (ArcPS && ArcPS->IsBot() == false)
			{
				Count++;
			}
		}
	}
 
	return Count;
}

int32 UArcaneRosterComponent::CountTeamStarts(int32 TeamID) const
{
	const FName WantedTag = FName(*FString::Printf(TEXT("Team%d"), TeamID));
	int32 Count = 0;
 
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		const APlayerStart* Start = *It;
		if (IsValid(Start) == false || Cast<APlayerStartPIE>(Start) != nullptr)
		{
			continue;
		}
 
		if (Start->PlayerStartTag == WantedTag)
		{
			Count++;
		}
	}
 
	return Count;
}

int32 UArcaneRosterComponent::ComputeTeamCapacityCeiling() const
{
	const AArcaneGameMode* GM = GetOwnerGM();
	if (GM == nullptr)
	{
		return 1;
	}
 
	const int32 TeamCount = FMath::Max(1, GM->GetNumTeams());
	int32 Ceiling = FMath::Max(1, GM->GetMaxPlayersPerTeamHardCap());
 
	for (int32 Team = 0; Team < TeamCount; Team++)
	{
		const int32 Starts = CountTeamStarts(Team);
		if (Starts > 0)
		{
			Ceiling = FMath::Min(Ceiling, Starts);
		}
	}
 
	return Ceiling;
}

bool UArcaneRosterComponent::IsStartSpotOccupied(APawn* PawnToFit, const FVector& Location,
	const FRotator& Rotation) const
{
	const AArcaneGameMode* GM = GetOwnerGM();
	UWorld* World = GetWorld();
	if (World == nullptr || GM == nullptr)
	{
		return false;
	}
 
	if (PawnToFit)
	{
		if (World->EncroachingBlockingGeometry(PawnToFit, Location, Rotation))
		{
			FVector Adjusted = Location;
			if (!World->FindTeleportSpot(PawnToFit, Adjusted, Rotation))
			{
				return true;
			}
		}
	}
 
	const float Radius = GM->GetSpawnOccupiedRadius();
	const float RadiusSq = FMath::Square(Radius);
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		const APawn* Other = *It;
		if (!IsValid(Other) || Other->IsPendingKillPending())
		{
			continue;
		}
 
		if (const AArcaneCharacter* ArcaneOther = Cast<AArcaneCharacter>(Other))
		{
			if (ArcaneOther->bIsDead)
			{
				continue;
			}
		}
 
		const FVector Delta = Other->GetActorLocation() - Location;
		if (Delta.SizeSquared2D() < RadiusSq && FMath::Abs(Delta.Z) < Radius)
		{
			return true;
		}
	}
 
	return false;
}

bool UArcaneRosterComponent::SpawnBot(int32 TeamID)
{
	AArcaneGameMode* GM = GetOwnerGM();
	UWorld* World = GetWorld();
	if (GM == nullptr || World == nullptr ||
		GM->GetBotControllerClass() == nullptr || GM->GetBotCharacterClass() == nullptr)
	{
		UE_LOG(LogArcane, Warning, TEXT("SpawnBot: BotControllerClass/BotCharacterClass 未在 GameMode 上配置"));
		return false;
	}
 
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 
	AArcaneAIController* Bot = World->SpawnActor<AArcaneAIController>(
		GM->GetBotControllerClass(), FTransform::Identity, Params);
 
	if (Bot == nullptr)
	{
		return false;
	}
 
	AArcanePlayerState* PS = Bot->GetPlayerState<AArcanePlayerState>();
	if (PS == nullptr)
	{
		UE_LOG(LogArcane, Warning, TEXT("SpawnBot: PlayerStateClass 不是 AArcanePlayerState？"));
		Bot->Destroy();
		return false;
	}
 
	PS->bIsBot = true;
	PS->TeamID = TeamID;
	PS->SetPlayerName(MakeBotName());
 
	Bot->SetGenericTeamId(FGenericTeamId(TeamID));
	GM->RestartPlayer(Bot);
 
	if (Bot->GetPawn() == nullptr)
	{
		UE_LOG(LogArcane, Warning, TEXT("SpawnBot: RestartPlayer 未能生成 Pawn（Team%d）"), TeamID);
		Bot->DestroyBotCompletely();
		return false;
	}
 
	NotifyRosterChanged();
	return true;
}

void UArcaneRosterComponent::DestroyAllBots()
{
	TArray<AArcaneAIController*> Bots;
 
	for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
	{
		if (AArcaneAIController* Bot = Cast<AArcaneAIController>(It->Get()))
		{
			Bots.Add(Bot);
		}
	}
 
	for (AArcaneAIController* Bot : Bots)
	{
		Bot->DestroyBotCompletely();
	}
 
	if (Bots.Num() > 0)
	{
		NotifyRosterChanged();
	}
}

bool UArcaneRosterComponent::RequestAddBot(AArcanePlayerState* Requester, int32 TeamID)
{
	const AArcaneGameMode* GM = GetOwnerGM();
	const AArcaneGameState* GS = GetWorld() ? GetWorld()->GetGameState<AArcaneGameState>() : nullptr;
 
	if (GM == nullptr || GS == nullptr ||
		GS->MatchPhase != EArcaneMatchPhase::Warmup ||
		Requester == nullptr || Requester->bIsRoomOwner == false)
	{
		return false;
	}
 
	if (TeamID < 0 || TeamID >= FMath::Max(1, GM->GetNumTeams()))
	{
		return false;
	}
 
	if (CountTeamMembers(TeamID) >= GM->GetMaxPlayersPerTeam())
	{
		UE_LOG(LogArcane, Log, TEXT("RequestAddBot: Team%d 已满（%d/%d，含 Bot）"),
			TeamID, CountTeamMembers(TeamID), GM->GetMaxPlayersPerTeam());
		return false;
	}
 
	return SpawnBot(TeamID);
}

bool UArcaneRosterComponent::RequestRemoveLastBot(AArcanePlayerState* Requester, int32 TeamID)
{
	const AArcaneGameState* GS = GetWorld() ? GetWorld()->GetGameState<AArcaneGameState>() : nullptr;
	if (GS == nullptr || GS->MatchPhase != EArcaneMatchPhase::Warmup ||
		Requester == nullptr || Requester->bIsRoomOwner == false)
	{
		return false;
	}
 
	if (RemoveOneBotFromTeam(TeamID) == false)
	{
		return false;
	}
 
	NotifyRosterChanged();
	return true;
}

bool UArcaneRosterComponent::RequestBalanceBots(AArcanePlayerState* Requester)
{
	const AArcaneGameMode* GM = GetOwnerGM();
	const AArcaneGameState* GS = GetWorld() ? GetWorld()->GetGameState<AArcaneGameState>() : nullptr;
 
	if (GM == nullptr || GS == nullptr ||
		GS->MatchPhase != EArcaneMatchPhase::Warmup ||
		Requester == nullptr || Requester->bIsRoomOwner == false)
	{
		return false;
	}
 
	const int32 TeamCount = FMath::Max(1, GM->GetNumTeams());
 
	int32 Target = 0;
	for (int32 Team = 0; Team < TeamCount; Team++)
	{
		Target = FMath::Max(Target, CountTeamMembers(Team));
	}
	Target = FMath::Min(Target, GM->GetMaxPlayersPerTeam());
 
	if (Target <= 0)
	{
		return false;
	}
 
	bool bSpawnedAny = false;
	for (int32 Team = 0; Team < TeamCount; Team++)
	{
		while (CountTeamMembers(Team) < Target)
		{
			if (SpawnBot(Team) == false)
			{
				break;
			}
			bSpawnedAny = true;
		}
	}
 
	return bSpawnedAny;
}

bool UArcaneRosterComponent::RequestSetTeamCapacity(AArcanePlayerState* Requester, int32 NewCapacity)
{
	AArcaneGameMode* GM = GetOwnerGM();
	AArcaneGameState* GS = GetWorld() ? GetWorld()->GetGameState<AArcaneGameState>() : nullptr;
 
	if (GM == nullptr || GS == nullptr ||
		GS->MatchPhase != EArcaneMatchPhase::Warmup ||
		Requester == nullptr || Requester->bIsRoomOwner == false)
	{
		return false;
	}
 
	const int32 TeamCount = FMath::Max(1, GM->GetNumTeams());
	const int32 EffectiveCap = ComputeTeamCapacityCeiling();
	GS->MaxPlayersPerTeamHardCap = EffectiveCap;
 
	const int32 Clamped = FMath::Clamp(NewCapacity, 1, EffectiveCap);
	if (Clamped == GM->GetMaxPlayersPerTeam())
	{
		if (Clamped != NewCapacity)
		{
			UE_LOG(LogArcane, Log, TEXT("SetTeamCapacity: %d 被出生点上限收窄为 %d（可用出生点不足）"),
				NewCapacity, Clamped);
		}
		GS->NotifyRosterChanged();
		RefreshSessionRoster();
		return false;
	}
 
	GM->SetMaxPlayersPerTeam(Clamped);
	GS->MaxPlayersPerTeam = Clamped;
	RefreshSessionRoster();
 
	for (int32 Team = 0; Team < TeamCount; Team++)
	{
		int32 Trimmed = 0;
		while (CountTeamMembers(Team) > GM->GetMaxPlayersPerTeam() && CountTeamBots(Team) > 0
			&& Trimmed < EffectiveCap && RequestRemoveLastBot(Requester, Team))
		{
			Trimmed++;
		}
 
		if (CountTeamMembers(Team) > GM->GetMaxPlayersPerTeam())
		{
			UE_LOG(LogArcane, Warning, TEXT("SetTeamCapacity: Team%d 真人 %d 已超新上限 %d（真人不会被自动移出）"),
				Team, CountTeamMembers(Team) - CountTeamBots(Team), GM->GetMaxPlayersPerTeam());
		}
	}
 
	GS->NotifyRosterChanged();
	UE_LOG(LogArcane, Log, TEXT("SetTeamCapacity: 房主 %s 把每队上限设为 %d（硬上限 %d / 出生点收窄后 %d）"),
		*Requester->GetPlayerName(), GM->GetMaxPlayersPerTeam(), GM->GetMaxPlayersPerTeamHardCap(), EffectiveCap);
 
	return true;
}

void UArcaneRosterComponent::RefreshSessionRoster(APlayerState* Exclude)
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr || Owner->GetNetMode() == NM_Client)
	{
		return;
	}
 
	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UArcaneSessionSubsystem* Sub = GI ? GI->GetSubsystem<UArcaneSessionSubsystem>() : nullptr;
	const AArcaneGameMode* GM = GetOwnerGM();
	if (Sub == nullptr || GM == nullptr)
	{
		return;
	}
 
	const int32 Capacity = FMath::Max(1, GM->GetNumTeams()) * FMath::Max(1, GM->GetMaxPlayersPerTeam());
	Sub->SyncArcaneSessionRoster(Capacity, CountConnectedRealPlayers(Exclude));
}

FString UArcaneRosterComponent::MakeBotName()
{
	BotNameCounter++;
	return FString::Printf(TEXT("Arcane Bot %d"), BotNameCounter);
}

AArcaneGameMode* UArcaneRosterComponent::GetOwnerGM() const
{
	return Cast<AArcaneGameMode>(GetOwner());
}

void UArcaneRosterComponent::NotifyRosterChanged()
{
	if (AArcaneGameState* GS = GetWorld() ? GetWorld()->GetGameState<AArcaneGameState>() : nullptr)
	{
		GS->NotifyRosterChanged();
	} 
}
