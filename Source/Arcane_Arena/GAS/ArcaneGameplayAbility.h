// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "ScalableFloat.h"
#include "Core/ArcaneAbilityTypes.h"
#include "ArcaneGameplayAbility.generated.h"

/**
 * 能力基类：共享冷却容器方案（见 Docs/05_GAS_共享冷却方案.md）
 * C++ 定义骨架（冷却注入逻辑），蓝图子类只填 CooldownDuration / CooldownTags / Cost GE。
 */
UCLASS()
class ARCANE_ARENA_API UArcaneGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UArcaneGameplayAbility();

	// 该能力对应的输入槽
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arcane|Input")
	EArcaneAbilityInputID InputID = EArcaneAbilityInputID::None;

protected:
	// 冷却时长（秒），FScalableFloat 支持按等级缩放；每个 GA 子类在 Class Defaults 配
	// 注入到共享 GE_Cooldown 的 SetByCaller(Data.Cooldown)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cooldown")
	FScalableFloat CooldownDuration;

	// 技能专属冷却标签，例如 "Ability.Cooldown.Dash"；运行时经 DynamicGrantedTags 注入
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cooldown")
	FGameplayTagContainer CooldownTags;

	// GetCooldownTags() 返回合并结果的临时容器（父类标签 + 本类标签）
	UPROPERTY(Transient)
	FGameplayTagContainer TempCooldownTags;

	//~ UGameplayAbility 覆写 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

public:
	// 合并"GE 静态标签 + 技能专属标签"，保证 CheckCooldown 检查的标签集与实际授予的一致
	virtual const FGameplayTagContainer* GetCooldownTags() const override;

	// 注入 SetByCaller 时长 + DynamicGrantedTags 后应用共享 GE_Cooldown
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;
};
