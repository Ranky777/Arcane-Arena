// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "ArcaneGameState.generated.h"

UENUM(BlueprintType)
enum class EArcaneMatchPhase : uint8
{
	Warmup,
	Playing,
	RoundEnd,
	MatchEnd
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnArcaneScoreChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnArcanePhaseChanged, EArcaneMatchPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRoundNumberChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnArcaneRosterChanged);

/**
 * 
 */
UCLASS()
class ARCANE_ARENA_API AArcaneGameState : public AGameState
{
	GENERATED_BODY()
	
public:
	AArcaneGameState();
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Phase, Category = "Arcane|Match")
	EArcaneMatchPhase MatchPhase = EArcaneMatchPhase::Warmup;
	
	// [0]=队A, [1]=队B
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_TeamScores, Category = "Arcane|Match")
	TArray<int32> TeamScores;
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RoundNumber, Category = "Arcane|Match")
	int32 RoundNumber = 0;
	
	// 本局胜方（MatchEnd 时有效，INDEX_NONE = 未定局）。结算面板显示用。
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_WinnerTeam, Category = "Arcane|Match")
	int32 MatchWinnerTeamID = INDEX_NONE;
	
	UPROPERTY(BlueprintAssignable, Category = "Arcane|Match")
	FOnArcaneScoreChanged OnScoreChanged;
	
	UPROPERTY(BlueprintAssignable, Category = "Arcane|Match")
	FOnArcanePhaseChanged OnPhaseChanged;
	
	UPROPERTY(BlueprintAssignable, Category = "Arcane|Match")
	FOnRoundNumberChanged OnRoundNumberChanged;
	
	UPROPERTY(BlueprintAssignable, Category = "Arcane|Match")
	FOnArcaneRosterChanged OnRosterChanged;
	
	// 名单版本号：任何成员加入/离开/换队/加删 Bot 都 +1，复制到所有客户端。
	// OnRosterChanged 是服务端本地委托，客户端收不到；Bot 由纯服务端生成，
	// 面板刷新必须靠这个复制属性的 OnRep 驱动
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RosterVersion, Category = "Arcane|Match")
	int32 RosterVersion = 0;
	
	// GameMode 侧 roster 变化的唯一调用入口（服务端增号并本地广播，客户端 OnRep 广播）
	void NotifyRosterChanged();
	
	// 每队人数上限的复制镜像：权威值在 GameMode（不复制，客户端根本不存在），
	// 服务端开局推进这里，UI 才能做"加 AI"按钮的禁用预判。
	// 走 OnRep 自己广播一次 OnRosterChanged：房主改上限时名单可能一个人都没变（RosterVersion 不动），
	// 而复制属性之间没有到达顺序保证，UI 刷新必须由"上限变了"这件事自己驱动。
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_TeamCapacity, Category = "Arcane|Match")
	int32 MaxPlayersPerTeam = 3;
	
	// 房主可调到的硬上限镜像：UI 靠它决定"+"号是否置灰（否则房主按到上限没有任何反馈）。
	// 权威值同样在 GameMode。
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_TeamCapacity, Category = "Arcane|Match")
	int32 MaxPlayersPerTeamHardCap = 7;
	
	// 先赢几局取胜（赛制）的复制镜像 + 可调天花板。权威值在 GameMode。
	// 同一个 OnRep 广播 OnRosterChanged：大厅面板靠它刷新数字，比赛中的回合标签
	// 则在下次 OnRoundNumberChanged（开局/换回合）时自然读到新值。
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MatchRules, Category = "Arcane|Match")
	int32 RoundsToWin = 2;
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MatchRules, Category = "Arcane|Match")
	int32 RoundsToWinMax = 7;

	// —— 蓝图侧阶段判断辅助 ——
	// 为什么不直接让蓝图比枚举：MatchPhase 是 uint8 枚举蓝图属性，
	// 蓝图里没有可靠的 == / Switch on Enum 节点可用（UK2Node_EnumEquality
	// 不暴露为可调用节点）。所以把判定收进 C++，蓝图只调一个返回 bool 的纯函数。
	UFUNCTION(BlueprintPure, Category = "Arcane|Match")
	bool IsPhase(EArcaneMatchPhase InPhase) const { return MatchPhase == InPhase; }

	// 本局胜方是否为本地玩家所在队（结算面板判「胜利/失败」用）
	UFUNCTION(BlueprintPure, Category = "Arcane|Match")
	bool IsLocalTeamWinner() const;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	UFUNCTION()
	void OnRep_Phase();
	
	UFUNCTION()
	void OnRep_TeamScores();
	
	UFUNCTION()
	void OnRep_RoundNumber();

	UFUNCTION()
	void OnRep_WinnerTeam();
	
	UFUNCTION()
	void OnRep_RosterVersion();
	
	UFUNCTION()
	void OnRep_TeamCapacity();
	
	UFUNCTION()
	void OnRep_MatchRules();
};
