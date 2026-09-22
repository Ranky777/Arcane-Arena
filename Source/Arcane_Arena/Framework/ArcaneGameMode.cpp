// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneGameMode.h"

#include "Arcane_Arena.h"   // LogArcane（必须排在本文件头之后，满足 IWYU 头顺序规则）
#include "ArcaneGameState.h"
#include "EngineUtils.h"
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
		Chosen = Super::ChoosePlayerStart(Player);
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
		
		// Bot 死亡（Character 端 ASC）走另一条路，Phase 6 后半段补
	}
}

bool AArcaneGameMode::IsTeamWiped(int32 TeamID) const
{
	for (APlayerState* PS : GameState->PlayerArray)
	{
		if (AArcanePlayerState* ArcPS = Cast<AArcanePlayerState>(PS))
		{
			if (ArcPS->TeamID == TeamID && ArcPS->bIsDead == false)
			{
				return false; // 还有人活着
			}
		}
	}
	
	return true;
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
	
	// 判定比赛结束
	if (GS->TeamScores.IsValidIndex(WinnerTeamID) && GS->TeamScores[WinnerTeamID] >= WinningScore)
	{
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
	
	GS->MatchPhase = EArcaneMatchPhase::Playing;
	GS->OnPhaseChanged.Broadcast(GS->MatchPhase);
	
	GS->RoundNumber++;
	GS->OnRoundNumberChanged.Broadcast();
	
	// 重生所有玩家（Phase 6.3 会换成按队伍出生点）
	for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = Cast<APlayerController>(It->Get()))
		{
			// 先销毁旧 Pawn（含尸体），否则 RestartPlayer 会因 PC 已持有 Pawn 而空跑
			if (APawn* OldPawn = PC->GetPawn())
			{
				PC->UnPossess();
				OldPawn->Destroy();
			}

			// 清掉 Login/上次出生缓存的 StartSpot，否则引擎 ShouldSpawnAtStartSpot()
			// 会永远复用同一个点，回合重生不会按占用情况重新分配
			PC->StartSpot = nullptr;

			RestartPlayer(PC);
		}
	}
	
	for (APlayerState* PS : GameState->PlayerArray)
	{
		if (AArcanePlayerState* ArcPS = Cast<AArcanePlayerState>(PS))
		{
			ArcPS->bIsDead = false;
			
			if (UArcaneAbilitySystemComponent* ASC = ArcPS->GetArcaneASC())
			{
				const float MaxHealth = ASC->GetNumericAttribute(UArcaneAttributeSet::GetMaxHealthAttribute());
				ASC->SetNumericAttributeBase(UArcaneAttributeSet::GetHealthAttribute(), MaxHealth);
				
				// TODO: 之后的各种需要恢复的属性都在这里
			}
		}
	}
}

void AArcaneGameMode::HandleMatchHasStarted()
{
	Super::HandleMatchHasStarted();
	
	AArcaneGameState* GS = GetGameState<AArcaneGameState>();
	if (GS != nullptr)
	{
		GS->MatchPhase = EArcaneMatchPhase::Playing;
		GS->RoundNumber = 1;
		GS->TeamScores = {0, 0};
		
		GS->OnPhaseChanged.Broadcast(GS->MatchPhase);
		GS->OnRoundNumberChanged.Broadcast();
		GS->OnScoreChanged.Broadcast();
	}
}

void AArcaneGameMode::HandleMatchHasEnded()
{
	Super::HandleMatchHasEnded();
}
