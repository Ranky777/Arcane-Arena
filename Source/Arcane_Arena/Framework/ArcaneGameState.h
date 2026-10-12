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
};
