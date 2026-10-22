// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneGameMode.h"

#include "Arcane_Arena.h"   // LogArcane（必须排在本文件头之后，满足 IWYU 头顺序规则）
#include "ArcaneGameState.h"
#include "EngineUtils.h"
#include "AI/ArcaneAIController.h"
#include "Character/ArcaneBotCharacter.h"
#include "Character/ArcaneCharacter.h"
#include "Engine/PlayerStartPIE.h"
#include "GameFramework/GameMode.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerStart.h"
#include "GAS/ArcaneAbilitySystemComponent.h"
#include "GAS/ArcaneAttributeSet.h"
#include "Online/ArcaneSessionSubsystem.h"
#include "Player/ArcaneHUD.h"
#include "Player/ArcanePlayerController.h"
#include "Player/ArcanePlayerState.h"

AArcaneGameMode::AArcaneGameMode()
{
	PlayerControllerClass = AArcanePlayerController::StaticClass();
	PlayerStateClass = AArcanePlayerState::StaticClass();
	GameStateClass = AArcaneGameState::StaticClass();
	HUDClass = AArcaneHUD::StaticClass();
	// DefaultPawnClass = nullptr;   // 先不管，由 BP_ArcaneGameMode 配置；避免误生成模板 Pawn
}

void AArcaneGameMode::PostLogin(APlayerController* NewPlayer)
{
	// —— 关键顺序：TeamID 必须在 Super::PostLogin 之前写好 ——
	// Super::PostLogin() → HandleStartingNewPlayer() → RestartPlayer() → FindPlayerStart()
	// → ChoosePlayerStart()（引擎 GameModeBase.cpp:1068/1180）。
	// 如果先调 Super，首次出生时 TeamID 还是哨兵值 -1，ChoosePlayerStart 会一律按 Team0 选点，
	// 结果就是所有玩家都刷在同一侧——这正是"分不到两边"的根因。
	if (AArcanePlayerState* PS = NewPlayer ? NewPlayer->GetPlayerState<AArcanePlayerState>() : nullptr)
	{
		PS->TeamID = PickTeamForNewPlayer();
		
		if (PS->TeamID == INDEX_NONE)
		{
			// 必须在这里顶位：下面 Super::PostLogin → RestartPlayer → ChoosePlayerStart
			// 会按 TeamID 选出生点，位置得先腾出来。
			// 也只有"真的没位置"才减 Bot，房主不会看到真人没来几个就掉 Bot。
			PS->TeamID = EvictBotForRealPlayer();
			if (GetGameState<AArcaneGameState>())
			{
				GetGameState<AArcaneGameState>()->NotifyRosterChanged();
			}
		}
		// 房主 = Listen Server 的本地玩家。
		// 判定放在服务端：服务端上只有房主自己的 PC 满足 IsLocalController()，
		// 远端客户端的 PC 在服务端视角下不是本地的。PIE 双开时天然只命中一个。
		PS->bIsRoomOwner = NewPlayer->IsLocalController() && GetNetMode() == NM_ListenServer;
		
		UE_LOG(LogArcane, Log, TEXT("PostLogin: %s 分配队伍 TeamID=%d"), *PS->GetPlayerName(), PS->TeamID);
	}
	else
	{
		UE_LOG(LogArcane, Warning, TEXT("PostLogin: 拿不到 AArcanePlayerState，本次不分配队伍"));
	}

	// —— 关键：清掉 Login 阶段缓存的出生点 ——
	// 引擎在 Login → InitNewPlayer → UpdatePlayerStartSpot（GameModeBase.cpp:787）时就会调用
	// FindPlayerStart → ChoosePlayerStart，此时 TeamID 还是 -1，选出的 Team0 点被缓存进
	// Player->StartSpot；随后 PostLogin → RestartPlayer → FindPlayerStart 里
	// ShouldSpawnAtStartSpot()（GameModeBase.cpp:1168）会直接复用缓存，不再咨询 ChoosePlayerStart。
	// 这里清空缓存，强制按刚分配的正确 TeamID 重新选点。
	NewPlayer->StartSpot = nullptr;

	Super::PostLogin(NewPlayer);
	
	if (AArcaneGameState* GS = GetGameState<AArcaneGameState>())
	{
		GS->NotifyRosterChanged();
	}
	
	RefreshSessionRoster();
}

void AArcaneGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);
	
	// 引擎顺序：AController::Destroyed → GameMode::Logout → 之后才 CleanupPlayerState，
	// 所以此刻退场玩家的 PlayerState 仍在 PlayerArray 里，必须显式排除
	RefreshSessionRoster(Exiting ? Exiting->PlayerState : nullptr);
}

void AArcaneGameMode::RefreshSessionRoster(APlayerState* Exclude)
{
	if (GetNetMode() == NM_Client)
	{
		return;
	}
	
	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UArcaneSessionSubsystem* Sub = GI ? GI->GetSubsystem<UArcaneSessionSubsystem>() : nullptr;
	if (Sub == nullptr)
	{
		return;
	}
	
	const int32 Capacity = FMath::Max(1, NumTeams) * FMath::Max(1, MaxPlayersPerTeam);
	Sub->SyncArcaneSessionRoster(Capacity, CountConnectedRealPlayers(Exclude));
}

int32 AArcaneGameMode::CountConnectedRealPlayers(APlayerState* Exclude) const
{
	const AArcaneGameState* GS = GetGameState<AArcaneGameState>();
	if (GS == nullptr)
	{
		return 0;
	}
	
	int32 Count = 0;
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (PS == nullptr || PS == Exclude || PS->IsInactive())
		{
			continue;
		}
		
		const AArcanePlayerState* ArcPS = Cast<AArcanePlayerState>(PS);
		// Bot 由服务器生成，不占网络连接槽，不计入
		if (ArcPS && ArcPS->IsBot() == false)
		{
			Count++;
		}
	}
	
	return Count;
}

int32 AArcaneGameMode::PickTeamForNewPlayer() const
{
	const int32 TeamCount = FMath::Max(1, NumTeams);

	TArray<int32> TeamSizes;
	TeamSizes.Init(0, TeamCount);

	// 本玩家的 PlayerState 已进 PlayerArray（APlayerState::PostInitializeComponents 里注册），
	// 但它的 TeamID 还是 -1，下面的范围检查会自动忽略它，不会把自己算进去。
	if (GameState)
	{
		for (APlayerState* PS : GameState->PlayerArray)
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
		if (TeamSizes[Team] >= FMath::Max(1, MaxPlayersPerTeam))
		{
			continue; // 满员的队不参与，由调用方决定顶 Bot
		}
		if (BestTeam == INDEX_NONE || TeamSizes[Team] < TeamSizes[BestTeam])
		{
			BestTeam = Team;
		}
	}

	return BestTeam;
}

int32 AArcaneGameMode::EvictBotForRealPlayer()
{
	const int32 TeamCount = FMath::Max(1, NumTeams);
	
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
	
	// 没有 Bot 可顶 = 两队真人均已达上限。这种房间对外空位已经是 0、列表里搜不到，
	// 走到这里只剩"刷新后瞬间被占满"的竞态，兜底进人最少的队并告警，不为此造新机制。
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

bool AArcaneGameMode::RemoveOneBotFromTeam(int32 TeamID)
{
	if (GameState == nullptr)
	{
		return false;
	}
	
	for (int32 i = GameState->PlayerArray.Num() - 1; i >= 0; i--)
	{
		const AArcanePlayerState* ArcPS = Cast<AArcanePlayerState>(GameState->PlayerArray[i]);
		if (ArcPS != nullptr && ArcPS->bIsBot && ArcPS->TeamID == TeamID)
		{
			if (AArcaneAIController* Bot = Cast<AArcaneAIController>(ArcPS->GetOwner()))
			{
				// 同步销毁：UnPossess → Pawn->Destroy → CleanupPlayerState → Destroy
				// 返回后 PlayerArray 和出生点都已经腾空，本次 PostLogin 就能直接用上
				Bot->DestroyBotCompletely();
				
				return true;
			}
		}
	}
	
	return false;
}

bool AArcaneGameMode::IsStartSpotOccupied(APawn* PawnToFit, const FVector& Location, const FRotator& Rotation) const
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	// ① 静态几何阻挡：撞墙/卡地板，且挪不开 → 占用
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

	// ② 动态 Pawn 占位：已经有角色站在这个点上 → 占用
	// （几何检测对刚生成、尚未落稳的 Pawn 不稳定，这里用距离兜底）
	const float RadiusSq = FMath::Square(SpawnOccupiedRadius);
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
				continue;   // 尸体：模板要 Delay(DeathRemovalTime) 才移除，不该挡住出生点
			}
		}

		const FVector Delta = Other->GetActorLocation() - Location;
		if (Delta.SizeSquared2D() < RadiusSq && FMath::Abs(Delta.Z) < SpawnOccupiedRadius)
		{
			return true;
		}
	}

	return false;
}

AActor* AArcaneGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	const AArcanePlayerState* ArcPS = Player ? Player->GetPlayerState<AArcanePlayerState>() : nullptr;
	const int32 TeamID = ArcPS ? ArcPS->TeamID : INDEX_NONE;

	// TeamID 未就绪（-1）时退化为 Team0，保证一定能出生，不会返回空让引擎落到 0,0,0
	const FName WantedTag = FName(*FString::Printf(TEXT("Team%d"), FMath::Max(TeamID, 0)));

	UClass* PawnClass = GetDefaultPawnClassForController(Player);
	APawn* PawnToFit = PawnClass ? PawnClass->GetDefaultObject<APawn>() : nullptr;

	TArray<APlayerStart*> Prefered;      // 本队 且 空
	TArray<APlayerStart*> OwnTeamTaken;  // 本队 但 被占（宁可叠在一起，也不送对面）
	TArray<APlayerStart*> WrongTeam;     // 他队 且 空
	TArray<APlayerStart*> Occupied;      // 他队 且 被占（最后兜底）
	
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		APlayerStart* Start = *It;
		
		if (Cast<APlayerStartPIE>(Start) != nullptr)
		{
			continue; // PIE "Play From Here" 专用点，别动（双 PIE 下防误选）
		}
		
		// 没标阵营的点不参与选点：它不属于任何队伍，选中它等于随机传送
		if (Start->PlayerStartTag.IsNone())
		{
			continue;
		}
		
		const bool bOwnTeam = (Start->PlayerStartTag == WantedTag);
		const FVector Location = Start->GetActorLocation();
		const FRotator Rotation = Start->GetActorRotation();
		
		if (IsStartSpotOccupied(PawnToFit, Location, Rotation))
		{
			(bOwnTeam ? OwnTeamTaken : Occupied).Add(Start);
		}
		else if (bOwnTeam)
		{
			Prefered.Add(Start);
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
	if (Prefered.Num() > 0)
	{
		Chosen = PickRandom(Prefered);
	}
	else if (OwnTeamTaken.Num()>0)
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
		Prefered.Num(), WrongTeam.Num(), Occupied.Num());

	return Chosen;
}

void AArcaneGameMode::NotifyCharacterDeath(AArcaneCharacter* DeadCharacter)
{
	if (MatchState == MatchState::WaitingPostMatch)
	{
		return;
	}
	
	// 只在 Playing 阶段判断回合结束
	AArcaneGameState* GS = GetGameState<AArcaneGameState>();
	if (GS == nullptr || GS->MatchPhase != EArcaneMatchPhase::Playing)
	{
		return;
	}
	
	if (AArcanePlayerState* PS = DeadCharacter->GetPlayerState<AArcanePlayerState>())
	{
		const int32 DeadTeam = PS->TeamID;
		
		if (IsTeamWiped(DeadTeam))
		{
			EndRound(DeadTeam == 0 ? 1 : 0);
		}
	}
}

void AArcaneGameMode::HandleTeamRequest(AArcanePlayerState* PS, int32 NewTeamID)
{
	if (PS == nullptr)
	{
		return;
	}
	
	AArcaneGameState* GS = GetGameState<AArcaneGameState>();
	if (GS == nullptr || GS->MatchPhase != EArcaneMatchPhase::Warmup)
	{
		return; // 只在选阵营阶段允许修改队伍
	}
	
	if (NewTeamID < 0 || NewTeamID >= FMath::Max(1, NumTeams) || NewTeamID == PS->TeamID)
	{
		return;
	}
	
	// 目标队伍人数上限检查（不包括自己）
	if (CountTeamMembers(NewTeamID) >= MaxPlayersPerTeam)
	{
		UE_LOG(LogArcane, Log, TEXT("HandleTeamRequest: %s 想进 Team%d，但已满(%d/%d)"),
			*PS->GetPlayerName(), NewTeamID, CountTeamMembers(NewTeamID), MaxPlayersPerTeam);
		return;
	}
	
	PS->TeamID = NewTeamID;
	
	// 换队立刻传送到新队出生点，让「选阵营」有直观反馈
	if (AArcaneCharacter* Character = Cast<AArcaneCharacter>(PS->GetPawn()))
	{
		TeleportCharacterToTeamStart(Character);
	}
	
	// 让两端客户端的阵营面板刷新人数
	GS->NotifyRosterChanged();
}

bool AArcaneGameMode::HandleStartMatchRequest(AController* Requester)
{
	AArcaneGameState* GS = GetGameState<AArcaneGameState>();
	if (GS == nullptr || GS->MatchPhase != EArcaneMatchPhase::Warmup)
	{
		return false;
	}
	
	const AArcanePlayerState* PS = Requester ? Requester->GetPlayerState<AArcanePlayerState>() : nullptr;
	if (PS == nullptr || PS->bIsRoomOwner == false)
	{
		return false; // 只有房主能够开始游戏
	}
	
	// 两队都必须有人，否则空队会被判团灭 / 白拿分
	const int32 Team0Count = CountTeamMembers(0);
	const int32 Team1Count = CountTeamMembers(1);
	
	if (Team0Count == 0 || Team1Count == 0)
	{
		UE_LOG(LogArcane, Warning, TEXT("HandleStartMatchRequest: 人数不足 (T0=%d, T1=%d)，无法开始"), Team0Count, Team1Count);
		return false;
	}
	
	EnterPlayingPhase();
	
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
	if (PS == nullptr || PS->bIsRoomOwner == false)
	{
		return false;
	}

	EnterPlayingPhase();   // 保留队伍分配，只清比分
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
	if (PS == nullptr || PS->bIsRoomOwner == false)
	{
		return false;
	}
	
	// 返回房间 = 回到选阵营阶段（Warmup），不是再开一局。
	// 这里以前误调 EnterPlayingPhase，于是"返回房间"后仍处于 Playing：玩家能自由走动、
	// HUD 也不会收起，选阵营面板反而不出现。
	EnterWarmupPhase();
	return true;
}

bool AArcaneGameMode::RequestAddBot(AArcanePlayerState* Requester, int32 TeamID)
{
	AArcaneGameState* GS = GetGameState<AArcaneGameState>();
	if (GS == nullptr || GS->MatchPhase != EArcaneMatchPhase::Warmup)
	{
		return false; // 只有选阵营阶段能够加入 AI
	}
	if (Requester == nullptr || Requester->bIsRoomOwner == false)
	{
		return false; // 只有房主能够加入 AI
	}
	
	if (TeamID < 0 || TeamID >= FMath::Max(1, NumTeams))
	{
		return false;
	}
	
	if (CountTeamMembers(TeamID) >= MaxPlayersPerTeam)
	{
		UE_LOG(LogArcane, Log, TEXT("RequestAddBot: Team%d 已满（%d/%d，含 Bot）"), TeamID, CountTeamMembers(TeamID), MaxPlayersPerTeam);
		return false;
	}
	
	return SpawnBot(TeamID);
}

bool AArcaneGameMode::RequestRemoveLastBot(AArcanePlayerState* Requester, int32 TeamID)
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
	
	if (RemoveOneBotFromTeam(TeamID) == false)
	{
		return false;
	}
	
	GS->NotifyRosterChanged();
	
	return true;
}

bool AArcaneGameMode::RequestBalanceBots(AArcanePlayerState* Requester)
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
	
	const int32 TeamCount = FMath::Max(1, NumTeams);
	
	// 目标 = 当前人数最多队的成员数（包含AI）
	int32 Target = 0;
	for (int32 Team = 0; Team < TeamCount; Team++)
	{
		Target = FMath::Max(Target, CountTeamMembers(Team));
	}
	Target = FMath::Min(Target, MaxPlayersPerTeam);
	
	if (Target <= 0)
	{
		return false; // 两队全空不配平
	}
	
	bool bSpawnedAny = false;
	for (int32 Team = 0; Team < TeamCount; Team++)
	{
		while (CountTeamMembers(Team) < Target)
		{
			if (SpawnBot(Team) == false)
			{
				break; // 生成失败即终止，不空转
			}
			
			bSpawnedAny = true;
		}
	}
	
	// UE_LOG(LogArcane, Warning, TEXT("RequestBalanceBots: 房主 %s 配平至每队 %d 人（本次%s）"),
	// 	*Requester->GetPlayerName(), Target, bSpawnedAny ? TEXT("有新增") : TEXT("无新增"));
	return bSpawnedAny;
}

int32 AArcaneGameMode::CountTeamStarts(int32 TeamID) const
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

int32 AArcaneGameMode::ComputeTeamCapacityCeiling() const
{
	// 收窄一层：任何一队可用出生点数不够，就不允许把上限抬到那个数以上，
	// 否则 ChoosePlayerStart 的"他队空位"兜底会把人送到对面（阵营正确性 > 不叠人）。
	const int32 TeamCount = FMath::Max(1, NumTeams);
	int32 Ceiling = FMath::Max(1, MaxPlayersPerTeamHardCap);
	
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

bool AArcaneGameMode::RequestSetTeamCapacity(AArcanePlayerState* Requester, int32 NewCapacity)
{
	AArcaneGameState* GS = GetGameState<AArcaneGameState>();
	if (GS == nullptr || GS->MatchPhase != EArcaneMatchPhase::Warmup)
	{
		return false; // 只有选阵营阶段能改房间规模
	}
	if (Requester == nullptr || Requester->bIsRoomOwner == false)
	{
		return false; // 只有房主能改
	}
	
	const int32 TeamCount = FMath::Max(1, NumTeams);
	const int32 EffectiveCap = ComputeTeamCapacityCeiling();
	
	// 镜像"实际可调上限"，让 UI 的"+"置灰线和服务端接受的边界完全一致：
	// 房主按到出生点撑不住的那一档，按钮会自己变灰，而不是按了没反应。
	GS->MaxPlayersPerTeamHardCap = EffectiveCap;
	
	const int32 Clamped = FMath::Clamp(NewCapacity, 1, EffectiveCap);
	if (Clamped == MaxPlayersPerTeam)
	{
		if (Clamped != NewCapacity)
		{
			UE_LOG(LogArcane, Log, TEXT("SetTeamCapacity: %d 被出生点上限收窄为 %d（可用出生点不足）"),
				NewCapacity, Clamped);
		}
		GS->NotifyRosterChanged(); // 值没变，但要把收窄后的天花板推给 UI
		RefreshSessionRoster();
		
		return false; // 没有变化，不改上限
	}
	
	MaxPlayersPerTeam = Clamped;
	GS->MaxPlayersPerTeam = Clamped;
	RefreshSessionRoster();
	
	// 降容：超员的部分只请 Bot，真人永远不被自动移出（移出真人需要另一套投票/断线语义）
	for (int32 Team = 0; Team < TeamCount; Team++)
	{
		int32 Trimmed = 0;
		while (CountTeamMembers(Team) > MaxPlayersPerTeam && CountTeamBots(Team) > 0
			&& Trimmed < EffectiveCap && RequestRemoveLastBot(Requester, Team))
		{
			Trimmed++;
		}
		
		if (CountTeamMembers(Team) > MaxPlayersPerTeam)
		{
			UE_LOG(LogArcane, Warning, TEXT("SetTeamCapacity: Team%d 真人 %d 已超新上限 %d（真人不会被自动移出）"),
				Team, CountTeamMembers(Team) - CountTeamBots(Team), MaxPlayersPerTeam);
		}
	}
	
	GS->NotifyRosterChanged();
	UE_LOG(LogArcane, Log, TEXT("SetTeamCapacity: 房主 %s 把每队上限设为 %d（硬上限 %d / 出生点收窄后 %d）"),
		*Requester->GetPlayerName(), MaxPlayersPerTeam, MaxPlayersPerTeamHardCap, EffectiveCap);
	
	return true;
}

bool AArcaneGameMode::RequestSetRoundsToWin(AArcanePlayerState* Requester, int32 NewRounds)
{
	AArcaneGameState* GS = GetGameState<AArcaneGameState>();
	if (GS == nullptr || GS->MatchPhase != EArcaneMatchPhase::Warmup)
	{
		return false; // 只有选阵营阶段能改赛制
	}
	if (Requester == nullptr || Requester->bIsRoomOwner == false)
	{
		return false; // 只有房主能改
	}
	
	const int32 Clamped = FMath::Clamp(NewRounds, 1, FMath::Max(1, WinningScoreHardCap));
	if (Clamped == WinningScore)
	{
		// 值没变也要把天花板推给客户端：否则房主在"+"上按到顶会以为按钮坏了
		GS->RoundsToWin = WinningScore;
		GS->RoundsToWinMax = FMath::Max(1, WinningScoreHardCap);
		GS->NotifyRosterChanged();
		return false;
	}
	
	WinningScore = Clamped;
	GS->RoundsToWin = WinningScore;
	GS->RoundsToWinMax = FMath::Max(1, WinningScoreHardCap);
	GS->NotifyRosterChanged();
	
	// 只改之后的判定：已经拿到的比分不动（中途改赛制不该抹掉战绩）
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

bool AArcaneGameMode::SpawnBot(int32 TeamID)
{
	if (BotControllerClass == nullptr || BotCharacterClass == nullptr)
	{
		UE_LOG(LogArcane, Warning, TEXT("SpawnBot: BotControllerClass/BotCharacterClass 未在 GameMode 上配置"));
		return false;
	}
	
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	
	// SpawnActor 返回时 PostInitializeComponents 已跑完 → PlayerState 已创建并进入 PlayerArray
	AArcaneAIController* Bot = GetWorld()->SpawnActor<AArcaneAIController>(BotControllerClass, FTransform::Identity, Params);
	
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
	
	// 必须在 RestartPlayer 之前写：ChoosePlayerStart 按 PS->TeamID 选 TeamN 出生点
	PS->bIsBot = true;
	PS->TeamID = TeamID;
	PS->SetPlayerName(MakeBotName());
	
	Bot->SetGenericTeamId(FGenericTeamId(TeamID));
	
	RestartPlayer(Bot);
	
	if (Bot->GetPawn() == nullptr)
	{
		UE_LOG(LogArcane, Warning, TEXT("SpawnBot: RestartPlayer 未能生成 Pawn（Team%d）"), TeamID);
		Bot->DestroyBotCompletely();
		return false;
	}
	
	if (AArcaneGameState* GS = GetGameState<AArcaneGameState>())
	{
		GS->NotifyRosterChanged();
	}
	
	return true;
}

int32 AArcaneGameMode::CountTeamBots(int32 TeamID) const
{
	int32 Count = 0;
	
	for (APlayerState* PS : GameState->PlayerArray)
	{
		if (const AArcanePlayerState* ArcPS = Cast<AArcanePlayerState>(PS))
		{
			if (ArcPS->bIsBot && ArcPS->TeamID == TeamID)
			{
				Count++;
			}
		}
	}
	
	return Count;
}

void AArcaneGameMode::DestroyAllBots()
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
		if (AArcaneGameState* GS = GetGameState<AArcaneGameState>())
		{
			GS->NotifyRosterChanged();
		}
	}
}

//
// void AArcaneGameMode::AutoFillBots()
// {
// 	if (AutoFillBotsPerTeam <= 0 || BotControllerClass == nullptr || BotCharacterClass == nullptr)
// 	{
// 		return;
// 	}
// 	
// 	const int32 TeamCount = FMath::Max(1, NumTeams);
// 	
// 	for (int32 Team = 0; Team < TeamCount; ++Team)
// 	{
// 		// 每队最多补 AutoFillBotsPerTeam 个 Bot；真人占的坑不挤（CountTeamMembers 含真人与 Bot）
// 		int32 Guard = AutoFillBotsPerTeam;
// 		
// 		while (Guard > 0 && CountTeamMembers(Team) < AutoFillBotsPerTeam)
// 		{
// 			if (SpawnBot(Team) == false)
// 			{
// 				break; // 生成失败绝不空转
// 			}
// 			
// 			Guard--;
// 		}
// 	}
// }

FString AArcaneGameMode::MakeBotName()
{
	BotNameCounter++;
	return FString::Printf(TEXT("Arcane Bot %d"), BotNameCounter);
}

void AArcaneGameMode::LockAllCharacters(bool bLock)
{
	for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
	{
		if (AArcaneCharacter* Character = Cast<AArcaneCharacter>(It->Get()->GetPawn()))
		{
			Character->SetMatchLocked(bLock);
		}
	}
}

void AArcaneGameMode::HandleMatchPhaseChanged(EArcaneMatchPhase NewPhase)
{
	LockAllCharacters(NewPhase != EArcaneMatchPhase::Playing);
}

void AArcaneGameMode::TeleportCharacterToTeamStart(AArcaneCharacter* Character)
{
	if (Character == nullptr)
	{
		return;
	}
	
	// 复用现有的 ChoosePlayerStart：它已经处理了「本队空位 / 他队空位 / 已占」三级兜底
	AActor* Start = ChoosePlayerStart(Character->GetController());
	if (Start != nullptr)
	{
		Character->TeleportTo(Start->GetActorLocation(), Start->GetActorRotation(), false, true);
	}
}

void AArcaneGameMode::RespawnAllPlayers()
{
	// 玩家与 Bot 共用一条重生通路：销毁旧 Pawn → RestartPlayer。
	// Pawn 类由 GetDefaultPawnClassForController 按 Controller 类型分流。
	// ASC 归 PlayerState：Bot 与玩家逐字相同地走下面的重置循环（回血/回能量/清冷却）。
	// Bot 的新 Pawn 携带全新 ASC（构造默认满血、无冷却），无需属性重置代码。
	for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
	{
		AController* Controller = It->Get();
		if (Controller == nullptr)
		{
			continue;
		}
		
		if (Cast<APlayerController>(Controller) == nullptr && Cast<AArcaneAIController>(Controller) == nullptr)
		{
			continue;
		}
		
		// 先销毁旧 Pawn（含尸体），否则 RestartPlayer 会因 PC 已持有 Pawn 而空跑
		if (APawn* OldPawn = Controller->GetPawn())
		{
			Controller->UnPossess();
			OldPawn->Destroy();
		}
		
		// 清掉 Login/上次出生缓存的 StartSpot，否则引擎 ShouldSpawnAtStartSpot()
		// 会永远复用同一个点，回合重生不会按占用情况重新分配
		// StartSpot 只在 PlayerController 上有登录期缓存问题；AI 每次都会走 ChoosePlayerStart
		if (APlayerController* PC = Cast<APlayerController>(Controller))
		{
			PC->StartSpot = nullptr;
		}
		
		RestartPlayer(Controller);
	}
	
	// 重置 PlayerState 上的标记与属性
	for (APlayerState* PS : GameState->PlayerArray)
	{
		if (AArcanePlayerState* ArcPS = Cast<AArcanePlayerState>(PS))
		{
			ArcPS->bIsDead = false;
			
			if (UArcaneAbilitySystemComponent* ASC = ArcPS->GetArcaneASC())
			{
				const float MaxHealth = ASC->GetNumericAttribute(UArcaneAttributeSet::GetMaxHealthAttribute());
				ASC->SetNumericAttributeBase(UArcaneAttributeSet::GetHealthAttribute(), MaxHealth);

				const float MaxEnergy = ASC->GetNumericAttribute(UArcaneAttributeSet::GetMaxEnergyAttribute());
				ASC->SetNumericAttributeBase(UArcaneAttributeSet::GetEnergyAttribute(), MaxEnergy);
				
				// 冷却 Tag 是运行时 DynamicGrantedTags，按 GE 资产 Tag 查不到；
				// 交给 ASC 用各能力 GetCooldownTags() 的并集来清。
				ASC->ClearAllCooldowns();
			}
		}
	}
}

void AArcaneGameMode::EnterPlayingPhase()
{
	AArcaneGameState* GS = GetGameState<AArcaneGameState>();
	if (GS == nullptr)
	{
		return;
	}
	
	GetWorldTimerManager().ClearTimer(RoundEndTimerHandle);
	
	GS->TeamScores = {0, 0};
	GS->RoundNumber = 1;
	GS->MatchWinnerTeamID = INDEX_NONE;
	GS->MatchPhase = EArcaneMatchPhase::Playing;
	
	RespawnAllPlayers();
	LockAllCharacters(false);
	
	GS->OnScoreChanged.Broadcast();
	GS->OnRoundNumberChanged.Broadcast();
	GS->OnPhaseChanged.Broadcast(GS->MatchPhase);
}

int32 AArcaneGameMode::CountTeamMembers(int32 InTeamID) const
{
	int32 Count = 0;
	
	for (APlayerState* PS : GameState->PlayerArray)
	{
		if (const AArcanePlayerState* ArcPS = Cast<AArcanePlayerState>(PS))
		{
			if (ArcPS->TeamID == InTeamID)
			{
				Count++;
			}
		}
	}
	
	return Count;
}

void AArcaneGameMode::EnterWarmupPhase()
{
	AArcaneGameState* GS = GetGameState<AArcaneGameState>();
	if (GS == nullptr)
	{
		return;
	}
	
	// 回大厅 = 名单重排：旧 Bot 全部清掉，按当前配置重补
	DestroyAllBots();
	
	GetWorldTimerManager().ClearTimer(RoundEndTimerHandle);
	
	GS->TeamScores = {0, 0};
	GS->RoundNumber = 0;
	GS->MatchWinnerTeamID = INDEX_NONE;
	GS->MatchPhase = EArcaneMatchPhase::Warmup;
	
	GS->OnScoreChanged.Broadcast();
	GS->OnRoundNumberChanged.Broadcast();
	GS->OnPhaseChanged.Broadcast(GS->MatchPhase);
	
	LockAllCharacters(true);
	
	// AutoFillBots();
}

bool AArcaneGameMode::IsTeamWiped(int32 TeamID) const
{
	int32 TotalCount = 0;
	int32 AliveCount = 0;
	
	for (APlayerState* PS : GameState->PlayerArray)
	{
		if (AArcanePlayerState* ArcPS = Cast<AArcanePlayerState>(PS))
		{
			if (ArcPS->TeamID == TeamID)
			{
				TotalCount++;
				if (ArcPS->bIsDead == false)
				{
					AliveCount++;
				}
			}
		}
	}
	
	// 一个成员都没有的队不算团灭。
	// 否则「房主单人进队、另一队空着」时，房主一死就把胜利判给空队，白送一分。
	if (TotalCount == 0)
	{
		return false;
	}
	
	return AliveCount == 0;
}

void AArcaneGameMode::EndRound(int32 WinnerTeamID)
{
	AArcaneGameState* GS = GetGameState<AArcaneGameState>();
	if (GS == nullptr)
	{
		return;
	}
	
	GS->MatchPhase = EArcaneMatchPhase::RoundEnd;
	GS->OnPhaseChanged.Broadcast(GS->MatchPhase);
	
	if (GS->TeamScores.IsValidIndex(WinnerTeamID))
	{
		GS->TeamScores[WinnerTeamID]++;
		
		GS->OnScoreChanged.Broadcast();
	}
	
	// 清掉上一个回合遗留的结束定时器，防止"再来一局"后旧定时器把新回合打断
	GetWorldTimerManager().ClearTimer(RoundEndTimerHandle);
	
	const bool bMatchOver = GS->TeamScores.IsValidIndex(WinnerTeamID) && GS->TeamScores[WinnerTeamID] >= WinningScore;
	
	// 判定比赛结束
	if (bMatchOver)
	{
		GS->MatchWinnerTeamID = WinnerTeamID;
		GS->MatchPhase = EArcaneMatchPhase::MatchEnd;
		
		GS->OnPhaseChanged.Broadcast(GS->MatchPhase);
		
		// [PLACEHOLDER] Phase 7：结算 UI / 回大厅
		return;
	}
	
	// 定时开启下一回合
	GetWorldTimerManager().SetTimer(RoundEndTimerHandle, this, &AArcaneGameMode::StartNextRound, RoundEndDuration, false);
}

void AArcaneGameMode::StartNextRound()
{
	AArcaneGameState* GS = GetGameState<AArcaneGameState>();
	if (GS == nullptr)
	{
		return;
	}
	
	RespawnAllPlayers();
	
	GS->MatchPhase = EArcaneMatchPhase::Playing;
	GS->MatchWinnerTeamID = INDEX_NONE;
	GS->RoundNumber++;
	GS->OnRoundNumberChanged.Broadcast();
	GS->OnPhaseChanged.Broadcast(GS->MatchPhase);
}

void AArcaneGameMode::HandleMatchHasStarted()
{
	Super::HandleMatchHasStarted();
	
	AArcaneGameState* GS = GetGameState<AArcaneGameState>();
	if (GS != nullptr)
	{
		// 锁定统一由阶段委托驱动：绑一次，之后任何 MatchPhase 变化都会重扫全场角色
		if (bPhaseChangedDelegateBound == false)
		{
			GS->OnPhaseChanged.AddDynamic(this, &AArcaneGameMode::HandleMatchPhaseChanged);
			bPhaseChangedDelegateBound = true;
		}
		
		GS->MaxPlayersPerTeam = MaxPlayersPerTeam;
		GS->MaxPlayersPerTeamHardCap = ComputeTeamCapacityCeiling();
		GS->RoundsToWin = WinningScore;
		GS->RoundsToWinMax = FMath::Max(1, WinningScoreHardCap);
		// 关键：连接完成 ≠ 开打。先进 Warmup（选阵营），等房主点开始。
		GS->MatchPhase = EArcaneMatchPhase::Warmup;
		GS->RoundNumber = 0;
		GS->TeamScores = {0, 0};
		GS->MatchWinnerTeamID = INDEX_NONE;
		
		GS->OnPhaseChanged.Broadcast(GS->MatchPhase);
		GS->OnRoundNumberChanged.Broadcast();
		GS->OnScoreChanged.Broadcast();
		
		// AutoFillBots();
	}
}

void AArcaneGameMode::HandleMatchHasEnded()
{
	Super::HandleMatchHasEnded();
}
