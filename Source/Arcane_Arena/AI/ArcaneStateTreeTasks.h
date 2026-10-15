#pragma once

#include "CoreMinimal.h"
#include "Core/ArcaneAbilityTypes.h"
#include "Tasks/StateTreeAITask.h"
#include "ArcaneStateTreeTasks.generated.h"

class UGameplayAbility;

/**
 * 攻击：面向目标 → 按下 InputID → 保持 HoldTime → 松开 → Succeeded。
 * 蓄力型 GA（WaitInputRelease）靠"按住再松"才会真正发射；点按型 HoldTime 给 0.1 即可。
 * 与玩家共用 AbilityLocalInputPressed/Released 同一条通道，尊重 GA 的输入语义。
 */
USTRUCT()
struct FArcaneTaskBotAttackInstanceData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = "Parameter")
	EArcaneAbilityInputID InputID = EArcaneAbilityInputID::None;
 
	// 按住时长（蓄力档）；点按型技能给 0.1 [PLACEHOLDER]
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0"))
	float HoldTime = 0.1f;
	
	UPROPERTY(Transient)
	float Elapsed = 0.0f;
     
	UPROPERTY(Transient)
	bool bPressed = false;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Arcane Bot Attack", Category = "AI|Action"))
struct FArcaneTaskBotAttack : public FStateTreeAITaskBase
{
	GENERATED_BODY()
	
	using FInstanceDataType = FArcaneTaskBotAttackInstanceData;
	
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); };
	
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT()
struct FArcaneTaskBotDashInstanceData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bDashTowardTarget = true;
};

/** Dash：朝(true)/背(false)目标转向后释放控制器的 DashAbilityClass */
USTRUCT(BlueprintType, meta = (DisplayName = "Arcane Bot Dash", Category = "AI|Action"))
struct FArcaneTaskBotDash : public FStateTreeAITaskBase
{
	GENERATED_BODY()
	
	using FInstanceDataType = FArcaneTaskBotDashInstanceData;
	
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); };
	
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT()
struct FArcaneTaskBotFaceInstanceData
{
	GENERATED_BODY()
	
	// 朝向由 focus 驱动，不需要参数；预留将来做"锁定目标来源"
};

USTRUCT(DisplayName = "Arcane Bot Face", Category = "AI|Action")
struct FArcaneTaskBotFace : public FStateTreeAITaskBase
{
	GENERATED_BODY()
	
	using FInstanceDataType = FArcaneTaskBotFaceInstanceData;
	
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
