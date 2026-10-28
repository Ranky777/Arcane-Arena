#pragma once

#include "CoreMinimal.h"
#include "Framework/ArcaneGameState.h"
#include "GameFramework/GameMode.h"
#include "ArcaneGameMode.generated.h"

class UArcaneRosterComponent;
class UArcaneMatchFlowComponent;
class AArcaneBotCharacter;
class AArcaneAIController;
class AArcaneCharacter;
class AArcanePlayerState;

UCLASS()
class ARCANE_ARENA_API AArcaneGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	AArcaneGameMode();

	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	// 角色（服务端）死亡时上报，回合系统唯一入口
	void NotifyCharacterDeath(AArcaneCharacter* DeadCharacter);

	// —— 房间/阵营阶段（客户端通过 Server RPC 调用，服务端在此校验） ——
	void HandleTeamRequest(AArcanePlayerState* PS, int32 NewTeamID);
	bool HandleStartMatchRequest(AController* Requester);
	bool HandleRematchRequest(AController* Requester);
	bool HandleBackToLobbyRequest(AController* Requester);

	// —— Bot / 房间规则请求 ——
	bool RequestAddBot(AArcanePlayerState* Requester, int32 TeamID);
	bool RequestRemoveLastBot(AArcanePlayerState* Requester, int32 TeamID);
	bool RequestBalanceBots(AArcanePlayerState* Requester);
	bool RequestSetTeamCapacity(AArcanePlayerState* Requester, int32 NewCapacity);
	bool RequestSetRoundsToWin(AArcanePlayerState* Requester, int32 NewRounds);

	// —— 配置（保持 BP_CombatGameMode 的既有 Class Defaults 不变） ——
	UPROPERTY(EditDefaultsOnly, Category = "Arcane|Match", meta = (ClampMin = "1"))
	int32 MaxPlayersPerTeam = 3;

	UPROPERTY(EditDefaultsOnly, Category = "Arcane|Match", meta = (ClampMin = "1"))
	int32 MaxPlayersPerTeamHardCap = 7;

	UPROPERTY(EditDefaultsOnly, Category = "Arcane|Match", meta = (ClampMin = "1"))
	int32 WinningScore = 2;

	UPROPERTY(EditDefaultsOnly, Category = "Arcane|Match", meta = (ClampMin = "1"))
	int32 WinningScoreHardCap = 7;

	UPROPERTY(EditDefaultsOnly, Category = "Arcane|Bot")
	TSubclassOf<AArcaneAIController> BotControllerClass;

	UPROPERTY(EditDefaultsOnly, Category = "Arcane|Bot")
	TSubclassOf<AArcaneBotCharacter> BotCharacterClass;

	UPROPERTY(EditDefaultsOnly, Category = "Arcane|Match")
	int32 NumTeams = 2;

	UPROPERTY(EditDefaultsOnly, Category = "Arcane|Match", meta = (ClampMin = "0.0"))
	float SpawnOccupiedRadius = 150.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Arcane|Match")
	float RoundEndDuration = 5.0f;

	// —— 组件 ——
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arcane|Match")
	TObjectPtr<UArcaneRosterComponent> RosterComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arcane|Match")
	TObjectPtr<UArcaneMatchFlowComponent> MatchFlowComponent;

	// —— 组件读取配置用的 getter ——
	int32 GetNumTeams() const { return NumTeams; }
	int32 GetMaxPlayersPerTeam() const { return MaxPlayersPerTeam; }
	int32 GetMaxPlayersPerTeamHardCap() const { return MaxPlayersPerTeamHardCap; }
	float GetSpawnOccupiedRadius() const { return SpawnOccupiedRadius; }
	int32 GetWinningScore() const { return WinningScore; }
	int32 GetWinningScoreHardCap() const { return WinningScoreHardCap; }
	float GetRoundEndDuration() const { return RoundEndDuration; }
	TSubclassOf<AArcaneAIController> GetBotControllerClass() const { return BotControllerClass; }
	TSubclassOf<AArcaneBotCharacter> GetBotCharacterClass() const { return BotCharacterClass; }
	void SetMaxPlayersPerTeam(int32 NewValue) { MaxPlayersPerTeam = NewValue; }

protected:
	virtual void HandleMatchHasStarted() override;
	virtual void HandleMatchHasEnded() override;

	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
};