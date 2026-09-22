// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "ArcaneGameMode.generated.h"

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
};
