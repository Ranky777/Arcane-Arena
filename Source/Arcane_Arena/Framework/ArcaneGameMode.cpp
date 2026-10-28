#include "ArcaneGameMode.h"

#include "Arcane_Arena.h"
#include "ArcaneGameState.h"
#include "ArcaneMatchFlowComponent.h"
#include "ArcaneRosterComponent.h"
#include "AI/ArcaneAIController.h"
#include "Character/ArcaneCharacter.h"
#include "Engine/PlayerStartPIE.h"
#include "EngineUtils.h"
#include "Character/ArcaneBotCharacter.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerStart.h"
#include "Player/ArcaneHUD.h"
#include "Player/ArcanePlayerController.h"
#include "Player/ArcanePlayerState.h"

AArcaneGameMode::AArcaneGameMode()
{
	RosterComponent = CreateDefaultSubobject<UArcaneRosterComponent>(TEXT("RosterComponent"));
	MatchFlowComponent = CreateDefaultSubobject<UArcaneMatchFlowComponent>(TEXT("MatchFlowComponent"));

	PlayerControllerClass = AArcanePlayerController::StaticClass();
	PlayerStateClass = AArcanePlayerState::StaticClass();
	GameStateClass = AArcaneGameState::StaticClass();
	HUDClass = AArcaneHUD::StaticClass();
}

void AArcaneGameMode::PostLogin(APlayerController* NewPlayer)
{
	// TeamID 必须在 Super::PostLogin 之前写好：
	// Super → RestartPlayer → ChoosePlayerStart 会按 TeamID 选出生点。
	if (AArcanePlayerState* PS = NewPlayer ? NewPlayer->GetPlayerState<AArcanePlayerState>() : nullptr)
	{
		PS->TeamID = RosterComponent ? RosterComponent->PickTeamForNewPlayer() : 0;

		if (PS->TeamID == INDEX_NONE && RosterComponent)
		{
			PS->TeamID = RosterComponent->EvictBotForRealPlayer();

			if (AArcaneGameState* GS = GetGameState<AArcaneGameState>())
			{
				GS->NotifyRosterChanged();
			}
		}

		PS->bIsRoomOwner = NewPlayer->IsLocalController() && GetNetMode() == NM_ListenServer;

		UE_LOG(LogArcane, Log, TEXT("PostLogin: %s 分配队伍 TeamID=%d"), *PS->GetPlayerName(), PS->TeamID);
	}
	else
	{
		UE_LOG(LogArcane, Warning, TEXT("PostLogin: 拿不到 AArcanePlayerState，本次不分配队伍"));
	}

	// 清掉 Login 阶段缓存的出生点，强制按刚分配的 TeamID 重新选点
	NewPlayer->StartSpot = nullptr;

	Super::PostLogin(NewPlayer);

	if (AArcaneGameState* GS = GetGameState<AArcaneGameState>())
	{
		GS->NotifyRosterChanged();
	}

	if (RosterComponent)
	{
		RosterComponent->RefreshSessionRoster();
	}
}

void AArcaneGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);

	// 此刻退场玩家的 PlayerState 仍在 PlayerArray 里，必须显式排除
	if (RosterComponent)
	{
		RosterComponent->RefreshSessionRoster(Exiting ? Exiting->PlayerState : nullptr);
	}
}

AActor* AArcaneGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	const AArcanePlayerState* ArcPS = Player ? Player->GetPlayerState<AArcanePlayerState>() : nullptr;
	const int32 TeamID = ArcPS ? ArcPS->TeamID : INDEX_NONE;

	// TeamID 未就绪（-1）时退化为 Team0，保证一定能出生
	const FName WantedTag = FName(*FString::Printf(TEXT("Team%d"), FMath::Max(TeamID, 0)));

	UClass* PawnClass = GetDefaultPawnClassForController(Player);
	APawn* PawnToFit = PawnClass ? PawnClass->GetDefaultObject<APawn>() : nullptr;

	TArray<APlayerStart*> Preferred;
	TArray<APlayerStart*> OwnTeamTaken;
	TArray<APlayerStart*> WrongTeam;
	TArray<APlayerStart*> Occupied;

	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		APlayerStart* Start = *It;

		if (Cast<APlayerStartPIE>(Start) != nullptr)
		{
			continue;
		}

		if (Start->PlayerStartTag.IsNone())
		{
			continue;
		}

		const bool bOwnTeam = (Start->PlayerStartTag == WantedTag);
		const FVector Location = Start->GetActorLocation();
		const FRotator Rotation = Start->GetActorRotation();

		const bool bOccupied = RosterComponent
			&& RosterComponent->IsStartSpotOccupied(PawnToFit, Location, Rotation);

		if (bOccupied)
		{
			(bOwnTeam ? OwnTeamTaken : Occupied).Add(Start);
		}
		else if (bOwnTeam)
		{
			Preferred.Add(Start);
		}
		else
		{
			WrongTeam.Add(Start);
		}
	}

	auto PickRandom = [](const TArray<APlayerStart*>& Starts) -> AActor*
	{
		return Starts[FMath::RandRange(0, Starts.Num() - 1)];
	};

	AActor* Chosen = nullptr;
	if (Preferred.Num() > 0)
	{
		Chosen = PickRandom(Preferred);
	}
	else if (OwnTeamTaken.Num() > 0)
	{
		Chosen = PickRandom(OwnTeamTaken);
	}
	else if (WrongTeam.Num() > 0)
	{
		Chosen = PickRandom(WrongTeam);
	}
	else if (Occupied.Num() > 0)
	{
		Chosen = PickRandom(Occupied);
	}
	else
	{
		Chosen = Super::ChoosePlayerStart_Implementation(Player);
	}

	UE_LOG(LogArcane, Log, TEXT("ChoosePlayerStart: %s(TeamID=%d) 目标Tag=%s -> %s (本队空位=%d/他队空位=%d/已占=%d)"),
		*GetNameSafe(Player), TeamID, *WantedTag.ToString(), *GetNameSafe(Chosen),
		Preferred.Num(), WrongTeam.Num(), Occupied.Num());

	return Chosen;
}

void AArcaneGameMode::NotifyCharacterDeath(AArcaneCharacter* DeadCharacter)
{
	if (MatchState == MatchState::WaitingPostMatch)
	{
		return;
	}

	if (MatchFlowComponent)
	{
		MatchFlowComponent->NotifyCharacterDeath(DeadCharacter);
	}
}

void AArcaneGameMode::HandleTeamRequest(AArcanePlayerState* PS, int32 NewTeamID)
{
	if (RosterComponent == nullptr || PS == nullptr)
	{
		return;
	}

	if (!RosterComponent->SetTeam(PS, NewTeamID))
	{
		return;
	}

	// 换队立刻传送到新队出生点
	if (MatchFlowComponent)
	{
		MatchFlowComponent->TeleportCharacterToTeamStart(Cast<AArcaneCharacter>(PS->GetPawn()));
	}

	if (AArcaneGameState* GS = GetGameState<AArcaneGameState>())
	{
		GS->NotifyRosterChanged();
	}
}

bool AArcaneGameMode::HandleStartMatchRequest(AController* Requester)
{
	AArcaneGameState* GS = GetGameState<AArcaneGameState>();
	if (GS == nullptr || GS->MatchPhase != EArcaneMatchPhase::Warmup || RosterComponent == nullptr)
	{
		return false;
	}

	const AArcanePlayerState* PS = Requester ? Requester->GetPlayerState<AArcanePlayerState>() : nullptr;
	if (PS == nullptr || PS->bIsRoomOwner == false)
	{
		return false;
	}

	const int32 Team0Count = RosterComponent->CountTeamMembers(0);
	const int32 Team1Count = RosterComponent->CountTeamMembers(1);

	if (Team0Count == 0 || Team1Count == 0)
	{
		UE_LOG(LogArcane, Warning, TEXT("HandleStartMatchRequest: 人数不足 (T0=%d, T1=%d)，无法开始"), Team0Count, Team1Count);
		return false;
	}

	if (MatchFlowComponent)
	{
		MatchFlowComponent->EnterPlayingPhase();
	}

	return true;
}

bool AArcaneGameMode::HandleRematchRequest(AController* Requester)
{
	AArcaneGameState* GS = GetGameState<AArcaneGameState>();
	if (GS == nullptr || GS->MatchPhase != EArcaneMatchPhase::MatchEnd)
	{
		return false;
	}

	const AArcanePlayerState* PS = Requester ? Requester->GetPlayerState<AArcanePlayerState>() : nullptr;
	if (PS == nullptr || PS->bIsRoomOwner == false || MatchFlowComponent == nullptr)
	{
		return false;
	}

	MatchFlowComponent->EnterPlayingPhase();
	return true;
}

bool AArcaneGameMode::HandleBackToLobbyRequest(AController* Requester)
{
	AArcaneGameState* GS = GetGameState<AArcaneGameState>();
	if (GS == nullptr || GS->MatchPhase != EArcaneMatchPhase::MatchEnd)
	{
		return false;
	}

	const AArcanePlayerState* PS = Requester ? Requester->GetPlayerState<AArcanePlayerState>() : nullptr;
	if (PS == nullptr || PS->bIsRoomOwner == false || MatchFlowComponent == nullptr)
	{
		return false;
	}

	MatchFlowComponent->EnterWarmupPhase();
	return true;
}

bool AArcaneGameMode::RequestAddBot(AArcanePlayerState* Requester, int32 TeamID)
{
	return RosterComponent && RosterComponent->RequestAddBot(Requester, TeamID);
}

bool AArcaneGameMode::RequestRemoveLastBot(AArcanePlayerState* Requester, int32 TeamID)
{
	return RosterComponent && RosterComponent->RequestRemoveLastBot(Requester, TeamID);
}

bool AArcaneGameMode::RequestBalanceBots(AArcanePlayerState* Requester)
{
	return RosterComponent && RosterComponent->RequestBalanceBots(Requester);
}

bool AArcaneGameMode::RequestSetTeamCapacity(AArcanePlayerState* Requester, int32 NewCapacity)
{
	return RosterComponent && RosterComponent->RequestSetTeamCapacity(Requester, NewCapacity);
}

bool AArcaneGameMode::RequestSetRoundsToWin(AArcanePlayerState* Requester, int32 NewRounds)
{
	AArcaneGameState* GS = GetGameState<AArcaneGameState>();
	if (GS == nullptr || GS->MatchPhase != EArcaneMatchPhase::Warmup)
	{
		return false;
	}
	if (Requester == nullptr || Requester->bIsRoomOwner == false)
	{
		return false;
	}

	const int32 Clamped = FMath::Clamp(NewRounds, 1, FMath::Max(1, WinningScoreHardCap));
	if (Clamped == WinningScore)
	{
		GS->RoundsToWin = WinningScore;
		GS->RoundsToWinMax = FMath::Max(1, WinningScoreHardCap);
		GS->NotifyRosterChanged();
		return false;
	}

	WinningScore = Clamped;
	GS->RoundsToWin = WinningScore;
	GS->RoundsToWinMax = FMath::Max(1, WinningScoreHardCap);
	GS->NotifyRosterChanged();

	UE_LOG(LogArcane, Log, TEXT("SetRoundsToWin: 房主 %s 把胜利局数设为先赢 %d 局（可调上限 %d，当前比分 %d:%d）"),
		*Requester->GetPlayerName(), WinningScore, WinningScoreHardCap,
		GS->TeamScores.IsValidIndex(0) ? GS->TeamScores[0] : 0,
		GS->TeamScores.IsValidIndex(1) ? GS->TeamScores[1] : 0);

	return true;
}

UClass* AArcaneGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	if (InController && InController->IsA<AArcaneAIController>() && BotCharacterClass != nullptr)
	{
		return BotCharacterClass;
	}

	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

void AArcaneGameMode::HandleMatchHasStarted()
{
	Super::HandleMatchHasStarted();

	if (MatchFlowComponent)
	{
		MatchFlowComponent->InitializeMatch();
	}
}

void AArcaneGameMode::HandleMatchHasEnded()
{
	Super::HandleMatchHasEnded();
}