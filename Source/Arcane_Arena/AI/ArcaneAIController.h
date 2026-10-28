// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Player/ArcanePlayerState.h"
#include "Runtime/AIModule/Classes/AIController.h"
#include "ArcaneAIController.generated.h"

enum class EArcaneAbilityInputID : uint8;
class UStateTreeAIComponent;
class UGameplayAbility;
class UAISenseConfig_Sight;
class AArcaneBotCharacter;

UCLASS()
class ARCANE_ARENA_API AArcaneAIController : public AAIController
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AArcaneAIController();
	
	AArcanePlayerState* GetArcanePlayerState() const { return Cast<AArcanePlayerState>(PlayerState); }
	
	AArcaneBotCharacter* GetBotCharacter() const;
	
	int32 GetArcaneTeamID() const;
	
	// 服务端移除 Bot 的唯一入口：Pawn → PlayerState → Controller。
	// APlayerState::Destroyed() 会自行出列 PlayerArray（引擎 PlayerState.cpp:204），
	// 客户端随 OnRep_PlayerArray 同步，不需要手工维护名单。
	void DestroyBotCompletely();
	
	// —— 9C 目标选择：StateTree / EQS 的唯一取敌入口 ——
	// 返回空 = 非 Playing 阶段 / 自己已死 / 视野与交战半径内无敌队成员。
	// 阶段门控只写在这一个函数里，行为侧不需要任何阶段特判。
	UFUNCTION(BlueprintPure, Category = "Arcane|AI")
	AActor* ResolveEnemyTarget() const;
	
	// —— StateTree 条件的数据源：把 GAS/PlayerState 的读取压平成标量，
	// 绑定表达式只做 Compare，不做链式取值
	UFUNCTION(BlueprintPure, Category = "Arcane|AI")
	bool IsSelfDead() const;
	
	UFUNCTION(BlueprintPure, Category = "Arcane|AI")
	float GetTargetDistance() const;
	
	// 敌我判定
	bool IsHostileCharacter(AActor* Actor) const;
	
	// 目标血量比例 0~1：AttackHeavy 效用曲线输入（满血值钱，残血归零）
	UFUNCTION(BlueprintPure, Category = "Arcane|UI")
	float GetTargetHealthFraction() const;
	
	// 技能已授予且冷却完毕（攻击/闪避状态的硬门槛；值不值得由曲线说话）
	UFUNCTION(BlueprintPure, Category = "Arcane|AI")
	bool IsAbilityReady(TSubclassOf<UGameplayAbility> AbilityClass) const;
	
	// 技能不仅要冷却好，还要付得起 DT_AbilityTable 里的 EnergyCost
	bool HasEnoughEnergyForAbility(TSubclassOf<UGameplayAbility> AbilityClass) const;
	
	// 能力是否已授予到 Bot 的 ASC。单独拆出来：诊断日志要区分"没授予"和"在冷却"两种失败
	bool HasAbilitySpec(TSubclassOf<UGameplayAbility> AbilityClass) const;
	
	// 该技能剩余冷却秒数；0 = 不在冷却（未授予/无输入槽/无冷却标签都返回 0）
	float GetAbilityCooldownRemaining(TSubclassOf<UGameplayAbility> AbilityClass) const;
	
	// 目标是否在指定技能的施法射程内。射程读 DT_AbilityTable，不在 AI 里复制一份 800
	UFUNCTION(BlueprintPure, Category = "Arcane|AI")
	bool IsTargetInAbilityRange(TSubclassOf<UGameplayAbility> AbilityClass) const;
	
	// 目标是否满足锁定条件（存活/敌对/无遮挡）；防止 Bot 隔墙交远程技能
	UFUNCTION(BlueprintPure, Category = "Arcane|AI")
	bool HasLineOfSightToTarget() const;
	
	// 每发远程技能在水平面内滚一个误差偏移（cm）。0 = 完美瞄准。
	// 只在服务器发射时调用，不复制；玩家路径不会走到这里。
	FVector RollHomingAimError() const;
	
	// 当前目标正在释放蓄力威胁技（GA 运行期间其 AssetTags 挂在目标 ASC 上）
	UFUNCTION(BlueprintPure, Category = "Arcane|AI")
	bool IsEnemyChargingThreat() const;
	
	/** 是否已进入近战位（单一事实源，避免 300 散落在多个 StateTree 状态里） */
	UFUNCTION(BlueprintPure, Category = "Arcane|Bot")
	bool IsInAttackRange() const;
	
	float GetAttackRange() const { return AttackRange; }
	
	TSubclassOf<UGameplayAbility> GetDashAbilityClass() const { return DashAbilityClass; };
	
	UFUNCTION(BlueprintPure, Category = "Arcane|AI")
	UArcaneAbilitySystemComponent* GetBotASC() const;
	
	/** 持续朝向：把自己挂到 focus 上，ControlRotation 每帧由引擎按目标当前位置算，天然跟随移动目标 */
	UFUNCTION(BlueprintCallable, Category = "Arcane|Bot")
	void TrackEnemyFacing(bool bTrack);
	
	// —— StateTree 任务的执行入口 ——
	// UFUNCTION(BlueprintCallable, Category = "Arcane|AI")
	// bool TryActivateBotAbilityClass(TSubclassOf<UGameplayAbility> AbilityClass);
	UFUNCTION(BlueprintCallable, Category = "Arcane|AI")
	bool SimulateAbilityInput(EArcaneAbilityInputID InputID, bool bPressed);
	
	// 转向当前目标（攻击前/冲刺前定向）；bFaceAway=背向（闪避）
	UFUNCTION(BlueprintCallable, Category = "Arcane|AI")
	bool FaceEnemyTarget(bool bFaceAway = false);
	
	// 朝/背目标转向后释放 DashAbilityClass（服务端权威）
	UFUNCTION(BlueprintCallable, Category = "Arcane|AI")
	bool TryBotDash(bool bTowardTarget);
	
protected:
	virtual void BeginPlay() override;
	
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	
	// GAS 自己报的"为什么这次激活被拒"（CheckCooldown/CheckCost/Tags/InputBlocked/BP CanUse）
	void OnBotAbilityFailed(const UGameplayAbility* Ability, const FGameplayTagContainer& FailureTags);
	
	FDelegateHandle BotAbilityFailedHandle;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arcane|AI")
	TObjectPtr<UAIPerceptionComponent> Perception;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arcane|AI")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arcane|AI")
	TObjectPtr<UStateTreeAIComponent> BotBrain;
	
	// 视觉之外兜一个"听觉"半径：防止 Bot 面壁罚站 [PLACEHOLDER]
	UPROPERTY(EditDefaultsOnly, Category = "Arcane|AI", meta = (ClampMin = "0.0"))
	float EngagementRadius = 2500.0f;
	
	/** 进入该距离即停下重新决策（Chase 出口 / 攻击进入共用同一个数）[PLACEHOLDER] */
	UPROPERTY(EditDefaultsOnly, Category = "Arcane|Bot", meta = (ClampMin = "0.0"))
	float AttackRange = 300.0f;
	
	// Dash 技能（BP_ArcaneAIController 默认值配置）
	UPROPERTY(EditDefaultsOnly, Category = "Arcane|Bot")
	TSubclassOf<UGameplayAbility> DashAbilityClass;
	
	// 远程技能瞄准误差半径（cm，水平面内均匀采样）。
	// 0 = 百发百中；有效命中半径约为“弹体半径 + 目标胶囊半径”（当前约 35 + 42 = 77cm）。
	UPROPERTY(EditDefaultsOnly, Category = "Arcane|Bot|Aim", meta = (ClampMin = "0.0"))
	float HomingAimErrorRadius = 100.0f;
	
	// 敌方"蓄力中"标记 = GA_SecondaryAttack 的 AssetTags 之一（GA 激活期间自动挂 ASC）
	UPROPERTY(EditDefaultsOnly, Category = "Arcane|Bot")
	FGameplayTag ChargingThreatTag;
};
