// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Framework/ArcaneGameState.h"
#include "GameFramework/GameMode.h"
#include "ArcaneGameMode.generated.h"

class AArcaneBotCharacter;
class AArcaneAIController;
class AArcaneCharacter;
class AArcanePlayerState;
class APawn;
/**
 * 
 */
UCLASS()
class ARCANE_ARENA_API AArcaneGameMode : public AGameMode
{
	GENERATED_BODY()
	
public:
	AArcaneGameMode();
	
	virtual void PostLogin(APlayerController* NewPlayer) override;
	
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	
	// 角色（服务端）死亡时上报，回合系统唯一入口
	void NotifyCharacterDeath(AArcaneCharacter* DeadCharacter);
	
	// —— 房间/阵营阶段（客户端通过 Server RPC 调用，服务端在此校验）——
	void HandleTeamRequest(AArcanePlayerState* PS, int32 NewTeamID);
	
	bool HandleStartMatchRequest(AController* Requester);
	
	bool HandleRematchRequest(AController* Requester);
	
	bool HandleBackToLobbyRequest(AController* Requester);
	
	// 每队人数上限 [PLACEHOLDER] 3v3
	UPROPERTY(EditDefaultsOnly, Category = "Arcane|Match", meta = (ClampMin = "1"))
	int32 MaxPlayersPerTeam = 3;
	
	// —— Bot：服务端权威。客户端唯一入口是 PlayerController 上的 Server RPC ——
	// 成功条件：Warmup 阶段 + 请求者是房主 + 目标队未满（Bot 计入满员数）
	bool RequestAddBot(AArcanePlayerState* Requester, int32 TeamID);
	
	bool RequestRemoveLastBot(AArcanePlayerState* Requester, int32 TeamID);
	
	// AI 快速配平：把落后的队补到与人数最多的队持平（钳制在 MaxPlayersPerTeam；不是补满）
	bool RequestBalanceBots(AArcanePlayerState* Requester);
	
	// Bot 类配置
	UPROPERTY(EditDefaultsOnly, Category = "Arcane|Bot")
	TSubclassOf<AArcaneAIController> BotControllerClass;
	
	UPROPERTY(EditDefaultsOnly, Category = "Arcane|Bot")
	TSubclassOf<AArcaneBotCharacter> BotCharacterClass;
	
	// // 进入 Warmup 时每队自动补的 Bot 数；0 = 关。单人离线验证 / 自动补位展示用 [PLACEHOLDER]
	// UPROPERTY(EditDefaultsOnly, Category = "Arcane|Bot", meta = (ClampMin = "0"))
	// int32 AutoFillBotsPerTeam = 0;
	
protected:
	/**
	 * 服务端：为刚进场的玩家挑队伍——"人少的队优先"，平手时取小号队。
	 * 等价于轮流分配，但玩家中途退出/重连时也能自动补平，不依赖登录序号。
	 */
	int32 PickTeamForNewPlayer() const;
	
	/**
	 * 服务端：出生点是否已被占用。
	 * 几何检测（EncroachingBlockingGeometry）对"刚生成、还没落稳的动态 Pawn"不敏感，
	 * 所以补一道距离判定，避免同队两个人被随机到同一个点叠在一起。
	 */
	bool IsStartSpotOccupied(APawn* PawnToFit, const FVector& Location, const FRotator& Rotation) const;
	
	// 服务端：检查某队是否团灭
	bool IsTeamWiped(int32 TeamID) const;
	
	// 回合推进
	void EndRound(int32 WinnerTeamID);
	
	void StartNextRound();
	
	virtual void HandleMatchHasStarted() override;
	
	virtual void HandleMatchHasEnded() override;
	
	int32 WinningScore = 2;   // 三局两胜 [PLACEHOLDER]

	// 队伍数量（3v3 → 2 队）。出生点 Tag 规则：Team0 / Team1 / ...
	UPROPERTY(EditDefaultsOnly, Category = "Arcane|Match")
	int32 NumTeams = 2;

	// 判定"出生点已被占用"的水平半径（同队两人不要叠一起）[PLACEHOLDER]
	UPROPERTY(EditDefaultsOnly, Category = "Arcane|Match", meta = (ClampMin = "0.0"))
	float SpawnOccupiedRadius = 150.0f;

	// 回合结束到下一回合的间隔
	UPROPERTY(EditDefaultsOnly, Category = "Arcane|Match")
	float RoundEndDuration = 5.0f;
	
	FTimerHandle RoundEndTimerHandle;
	
	// 服务端：把角色锁住（不能移动/放技能）或解锁
	void LockAllCharacters(bool bLock);
	
	// 服务端：把角色送到本队（TeamID）的空出生点，复用 ChoosePlayerStart 的占用判定
	// 注意：不能带 const —— ChoosePlayerStart 是 AGameModeBase 的非 const 成员
	void TeleportCharacterToTeamStart(AArcaneCharacter* Character);
	
	// 服务端：清空所有玩家状态并重生（回合重置的唯一实现）
	void RespawnAllPlayers();
	
	// 服务端：进入「PLAYING」状态的统一入口（清比分 + 重生 + 解锁 + 广播）
	void EnterPlayingPhase();
	
	/**
	 * 服务端：阶段变化 → 全场锁定状态的唯一入口（AddDynamic 绑到 GameState.OnPhaseChanged）。
	 * 只有 Playing 放开移动，Warmup / RoundEnd / MatchEnd 一律锁死。
	 * 以前是各流程手写 LockAllCharacters，结果 HandleMatchHasStarted、EndRound、
	 * StartNextRound 三处都漏了 —— 开局选阵营阶段能随意走动就是这里来的。
	 */
	UFUNCTION()
	void HandleMatchPhaseChanged(EArcaneMatchPhase NewPhase);
	
	// OnPhaseChanged 只绑一次（GameMode 与 GameState 同生命周期，无需解绑）
	bool bPhaseChangedDelegateBound = false;
	
	// 服务端：队伍人数统计
	int32 CountTeamMembers(int32 InTeamID) const;
	
	// 服务端：把阶段设为 Warmup 并重新上锁 + 清比分
	void EnterWarmupPhase();
	
	// AI Controller 取 BotCharacterClass，其余走引擎默认（玩家 DefaultPawnClass）
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
	
private:
	bool SpawnBot(int32 TeamID);
	
	int32 CountTeamBots(int32 TeamID) const;
	
	void DestroyAllBots();
	
	// void AutoFillBots();
	
	FString MakeBotName();
	
	int32 BotNameCounter = 0;
};
