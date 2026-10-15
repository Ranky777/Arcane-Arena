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

	int32 BestTeam = 0;
	for (int32 Team = 1; Team < TeamCount; ++Team)
	{
		if (TeamSizes[Team] < TeamSizes[BestTeam])
		{
			BestTeam = Team;
		}
	}

	return BestTeam;
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
		if (!IsValid(Other))
		{
			continue;
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

	TArray<APlayerStart*> Prefered; // 本队的空位
	TArray<APlayerStart*> Occupied;    // 被占（几何/距离判定），兜底
	TArray<APlayerStart*> WrongTeam;   // 空的但不是本队，第二兜底

	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		APlayerStart* Start = *It;
		
		if (Cast<APlayerStartPIE>(Start) != nullptr)
		{
			continue; // PIE "Play From Here" 专用点，别动（双 PIE 下防误选）
		}
		
		const FVector Location = Start->GetActorLocation();
		const FRotator Rotation = Start->GetActorRotation();
		
		if (IsStartSpotOccupied(PawnToFit, Location, Rotation))
		{
			Occupied.Add(Start);
		}
		else if (Start->PlayerStartTag == WantedTag)
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
	
	for (int32 i = GameState->PlayerArray.Num() - 1; i >= 0; i--)
	{
		const AArcanePlayerState* ArcPS = Cast<AArcanePlayerState>(GameState->PlayerArray[i]);
		if (ArcPS != nullptr && ArcPS->bIsBot && ArcPS->TeamID == TeamID)
		{
			if (AArcaneAIController* Bot = Cast<AArcaneAIController>(ArcPS->GetOwner()))
			{
				Bot->DestroyBotCompletely();
				GS->NotifyRosterChanged();
				
				return true;
			}
		}
	}
	
	return false;
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

				// 清除全部冷却
				const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyEffectTags(
					FGameplayTagContainer(FGameplayTag::RequestGameplayTag(FName("Ability.Cooldown"))));

				for (const FActiveGameplayEffectHandle& Handle : ASC->GetActiveEffects(Query))
				{
					ASC->RemoveActiveGameplayEffect(Handle);
				}
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
