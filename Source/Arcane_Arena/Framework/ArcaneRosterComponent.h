// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ArcaneRosterComponent.generated.h"

class AArcaneGameMode;
class AArcanePlayerState;
/**
 * 服务端房间名单组件：
 * - 分队 / 顶 Bot / 人数统计
 * - Bot 生成、销毁、配平
 * - 出生点数量与容量上限
 * - 会话名册同步
 *
 * 配置仍留在 AArcaneGameMode（MaxPlayersPerTeam / NumTeams / BotClass...），
 * 本组件通过 Owner 的 getter 读取，避免迁移 BP Class Defaults。
 */
UCLASS(ClassGroup=(Arcane), meta=(BlueprintSpawnableComponent))
class ARCANE_ARENA_API UArcaneRosterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UArcaneRosterComponent();
	
	// —— 分队 ——
	int32 PickTeamForNewPlayer() const;
	int32 EvictBotForRealPlayer();
	bool SetTeam(AArcanePlayerState* PS, int32 NewTeamID);
	bool RemoveOneBotFromTeam(int32 TeamID);
	
	// —— 统计 ——
	int32 CountTeamMembers(int32 TeamID) const;
	int32 CountTeamBots(int32 TeamID) const;
	int32 CountConnectedRealPlayers(APlayerState* Exclude = nullptr) const;
	
	// —— 出生点 / 容量 ——
	int32 CountTeamStarts(int32 TeamID) const;
	int32 ComputeTeamCapacityCeiling() const;
	bool IsStartSpotOccupied(APawn* PawnToFit, const FVector& Location, const FRotator& Rotation) const;
	
	// —— Bot 请求 ——
	bool SpawnBot(int32 TeamID);
	void DestroyAllBots();
	bool RequestAddBot(AArcanePlayerState* Requester, int32 TeamID);
	bool RequestRemoveLastBot(AArcanePlayerState* Requester, int32 TeamID);
	bool RequestBalanceBots(AArcanePlayerState* Requester);
	
	// —— 房间规则 ——
	bool RequestSetTeamCapacity(AArcanePlayerState* Requester, int32 NewCapacity);
	
	// —— 会话名册 ——
	void RefreshSessionRoster(APlayerState* Exclude = nullptr);
	
	FString MakeBotName();
	
private:
	AArcaneGameMode* GetOwnerGM() const;
	void NotifyRosterChanged();
	
	int32 BotNameCounter = 0;
};
