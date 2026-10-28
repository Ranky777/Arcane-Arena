#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Framework/ArcaneGameState.h"
#include "ArcaneMatchFlowComponent.generated.h"

class AArcaneCharacter;
class AArcaneGameMode;

/**
 * 服务端回合流程组件：
 * - Warmup / Playing / RoundEnd / MatchEnd 阶段切换
 * - 比分、回合数、回合结束定时器
 * - 回合重生、全场锁定、团灭判定
 * - 阶段变化驱动锁定
 */
UCLASS(ClassGroup = (Arcane), meta = (BlueprintSpawnableComponent))
class ARCANE_ARENA_API UArcaneMatchFlowComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UArcaneMatchFlowComponent();

	// GameMode.HandleMatchHasStarted 调用：绑委托 + 初始化 Warmup
	void InitializeMatch();

	void NotifyCharacterDeath(AArcaneCharacter* DeadCharacter);

	void EnterPlayingPhase();
	void EnterWarmupPhase();

	void EndRound(int32 WinnerTeamID);
	void StartNextRound();

	void RespawnAllPlayers();
	void LockAllCharacters(bool bLock);
	void TeleportCharacterToTeamStart(AArcaneCharacter* Character);

	bool IsTeamWiped(int32 TeamID) const;

	UFUNCTION()
	void HandleMatchPhaseChanged(EArcaneMatchPhase NewPhase);

private:
	AArcaneGameMode* GetOwnerGM() const;

	FTimerHandle RoundEndTimerHandle;

	// OnPhaseChanged 只绑一次
	bool bPhaseChangedDelegateBound = false;
};