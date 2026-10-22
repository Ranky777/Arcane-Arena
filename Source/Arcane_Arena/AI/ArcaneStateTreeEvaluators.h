#pragma once

#include "CoreMinimal.h"
#include "StateTreeEvaluatorBase.h"
#include "ArcaneStateTreeEvaluators.generated.h"

class UGameplayAbility;

USTRUCT()
struct FArcaneBotFactsInstanceData
{
	GENERATED_BODY()
	
	// —— 配置（ST 资产里填一次）：就绪性检查要盯的三个技能 ——
	UPROPERTY(EditAnywhere, Category = "Parameter")
	TSubclassOf<UGameplayAbility> PrimaryAbility;
	
	UPROPERTY(EditAnywhere, Category = "Parameter")
	TSubclassOf<UGameplayAbility> SecondaryAbility;
	
	UPROPERTY(EditAnywhere, Category = "Parameter")
	TSubclassOf<UGameplayAbility> HomingAbility;
	
	// —— 计算结果（每帧 Tick 刷新；条件/考虑项从这里绑）——
	UPROPERTY(EditAnywhere, Category = "Default")
	const AActor* TargetActor = nullptr;
	
	UPROPERTY(EditAnywhere, Category = "Default")
	float TargetDistance = TNumericLimits<float>::Max();
	
	UPROPERTY(EditAnywhere, Category = "Default")
	float TargetHealthFraction = 1.0f;
	
	UPROPERTY(EditAnywhere, Category = "Default")
	bool bHasTarget = false;
	
	UPROPERTY(VisibleAnywhere, Category = "Target")
	bool bInAttackRange = false;
	
	UPROPERTY(EditAnywhere, Category = "Default")
	bool bSelfDead = false;
	
	UPROPERTY(EditAnywhere, Category = "Default")
	bool bEnemyChargingThreat = false;
	
	UPROPERTY(EditAnywhere, Category = "Default")
	bool bPrimaryReady = false;
	
	UPROPERTY(EditAnywhere, Category = "Default")
	bool bSecondaryReady = false;
	
	UPROPERTY(EditAnywhere, Category = "Default")
	bool bHomingReady = false;
	
	UPROPERTY(EditAnywhere, Category = "Default")
	bool bHomingInRange = false;
	
	UPROPERTY(EditAnywhere, Category = "Default")
	bool bHomingHasLineOfSight = false;
	
	UPROPERTY(EditAnywhere, Category = "Default")
	bool bDashReady = false;
	
	// Arcane.AI.BotDebug 快照节流计时（仅调试用，不序列化）
	UPROPERTY(Transient)
	float DebugSnapshotTimer = 0.0f;
};

/**
 * 每帧把 Bot 的决策事实算好一份，供全树绑定——
 * 条件/效用各绑各的函数会重复穿透 GAS/感知，Evaluator 统一算一次。
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Arcane Bot Facts", Category = "AI"))
struct FArcaneBotFactsEvaluator : public FStateTreeEvaluatorCommonBase
{
	GENERATED_BODY()
	
	using FInstanceDataType = FArcaneBotFactsInstanceData;
	
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); };
	
	virtual void Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};
