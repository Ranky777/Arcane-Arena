// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneGameplayAbility.h"
#include "GameplayEffect.h"
#include "AbilitySystemComponent.h"

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

	// 追加本能力专属标签
	MutableTags->AppendTags(CooldownTags);
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

	// 1. 为共享冷却 GE 构建 Spec
	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(
		Handle, ActorInfo, ActivationInfo,
		CooldownGE->GetClass(), GetAbilityLevel(Handle, ActorInfo));
	if (!SpecHandle.IsValid())
	{
		return;
	}

	// 2. 动态注入技能专属冷却标签
	//    ⚠️ 必须用 DynamicGrantedTags（会授予到 ASC，CheckCooldown 靠它阻塞）
	//    不能用 AddDynamicAssetTag（仅 Spec 元数据，不授予，冷却会失效）
	SpecHandle.Data->DynamicGrantedTags.AppendTags(CooldownTags);

	// 3. SetByCaller 动态注入冷却时长
	SpecHandle.Data->SetSetByCallerMagnitude(
		FGameplayTag::RequestGameplayTag(FName("Data.Cooldown")),
		CooldownDuration.GetValueAtLevel(GetAbilityLevel(Handle, ActorInfo)));

	// 4. 应用到 Owner
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
}
