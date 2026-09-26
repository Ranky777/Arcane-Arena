// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneGameplayAbility.h"
#include "GameplayEffect.h"
#include "AbilitySystemComponent.h"
#include "Combat/ArcaneCombatData.h"
#include "Combat/ArcaneCombatDataLibrary.h"

UArcaneGameplayAbility::UArcaneGameplayAbility()
{
	// 预测策略默认：本地预测（手感优先）。ServerInitiated 的能力在 BP 子类里覆盖
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

const FGameplayTagContainer* UArcaneGameplayAbility::GetCooldownTags() const
{
	FGameplayTagContainer* MutableTags = const_cast<FGameplayTagContainer*>(&TempCooldownTags);
	MutableTags->Reset();

	// 父类结果 = 冷却 GE 的静态标签（Combined Tags）+ 引擎自带的 CooldownTags
	if (const FGameplayTagContainer* ParentTags = Super::GetCooldownTags())
	{
		MutableTags->AppendTags(*ParentTags);
	}

	// 追加 DT_AbilityTable 行内配置的专属冷却标签
	bool bFound = false;
	const FArcaneAbilityRow Row = UArcaneCombatDataLibrary::GetAbilityRow(AbilityID, bFound);
	if (bFound && Row.CooldownTag.IsValid())
	{
		MutableTags->AddTag(Row.CooldownTag);
	}
	else if (!bFound)
	{
		ensureMsgf(false, TEXT("[GetCooldownTags] AbilityID '%s' 在 AbilityTable 无对应行，冷却标签为空"), *AbilityID.ToString());
	}
	return MutableTags;
}

void UArcaneGameplayAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	UGameplayEffect* CooldownGE = GetCooldownGameplayEffect();
	if (!CooldownGE)
	{
		return;
	}

	// 严格数据驱动：冷却数值只来自表，缺行直接报警拒绝
	bool bFound = false;
	const FArcaneAbilityRow Row = UArcaneCombatDataLibrary::GetAbilityRow(AbilityID, bFound);
	if (!bFound)
	{
		ensureAlwaysMsgf(false, TEXT("[ApplyCooldown] AbilityID '%s' 在 AbilityTable 无对应行，冷却未注入"), *AbilityID.ToString());
		return;
	}

	// 1. 为共享冷却 GE 构建 Spec
	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(
		Handle, ActorInfo, ActivationInfo,
		CooldownGE->GetClass(), GetAbilityLevel(Handle, ActorInfo));
	if (!SpecHandle.IsValid())
	{
		return;
	}

	// 2. 动态注入技能专属冷却标签
	if (Row.CooldownTag.IsValid())
	{
		SpecHandle.Data->DynamicGrantedTags.AddTag(Row.CooldownTag);
	}

	// 3. SetByCaller 动态注入冷却时长
	SpecHandle.Data->SetSetByCallerMagnitude(
		FGameplayTag::RequestGameplayTag(FName("Data.Cooldown")), Row.CooldownDuration);

	// 4. 应用到 Owner
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
}

void UArcaneGameplayAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	UGameplayEffect* CostGE = GetCostGameplayEffect();
	if (!CostGE)
	{
		return;
	}
	
	bool bFound = false;
	const FArcaneAbilityRow Row = UArcaneCombatDataLibrary::GetAbilityRow(AbilityID, bFound);
	if (!bFound)
	{
		// 表缺行时宁可报警不扣能，也别静默
		ensureAlwaysMsgf(false, TEXT("[ApplyCost] AbilityID '%s' 在 AbilityTable 无对应行，Cost 未注入"), *AbilityID.ToString());
		return;
	}
	
	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(
		Handle, ActorInfo, ActivationInfo, CostGE->GetClass(), GetAbilityLevel(Handle, ActorInfo));
	if (!SpecHandle.IsValid())
	{
		return;
	}
	
	SpecHandle.Data->SetSetByCallerMagnitude(
		FGameplayTag::RequestGameplayTag(FName("Data.Cost")), -Row.EnergyCost);
	
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
}
