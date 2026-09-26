// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "Core/ArcaneAbilityTypes.h"
#include "ArcaneGameplayAbility.generated.h"

/**
 * 能力基类：共享冷却容器方案（见 Docs/05_GAS_共享冷却方案.md）+ 数据驱动（见 Docs/09）。
 * 冷却时长/冷却标签/耗能一律从 DT_AbilityTable 读（按 AbilityID 查行），
 * 蓝图子类只填：AbilityID + Cooldown Gameplay Effect Class(GE_Cooldown) + Cost Gameplay Effect Class(GE_Cost)。
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

	// 该能力在 DT_AbilityTable 里的行名；Class Defaults 填 PrimaryAttack / SecondaryAttack / Dash
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arcane|Data")
	FName AbilityID;

	// GetCooldownTags() 返回合并结果的临时容器（父类标签 + 表内标签）
	UPROPERTY(Transient)
	FGameplayTagContainer TempCooldownTags;

	//~ UGameplayAbility 覆写 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

public:
	// 合并"GE 静态标签 + 表内 CooldownTag"，保证 CheckCooldown 检查的标签集与实际授予的一致
	virtual const FGameplayTagContainer* GetCooldownTags() const override;

	// 注入 SetByCaller 时长 + DynamicGrantedTags 后应用共享 GE_Cooldown
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;
	
	// 注入 SetByCaller(Data.Cost) = -EnergyCost（从注册表读行）
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;
};
