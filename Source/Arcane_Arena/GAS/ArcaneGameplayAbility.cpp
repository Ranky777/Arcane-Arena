// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneGameplayAbility.h"
#include "GameplayEffect.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Arcane_Arena.h"
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

bool UArcaneGameplayAbility::CheckCost(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	UGameplayEffect* CostGE = GetCostGameplayEffect();
	if (CostGE == nullptr)
	{
		return true;
	}
	
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (ASC == nullptr)
	{
		return false;
	}
	
	bool bFound = false;
	const FArcaneAbilityRow Row = UArcaneCombatDataLibrary::GetAbilityRow(AbilityID, bFound);
	if (!bFound)
	{
		ensureAlwaysMsgf(false, TEXT("[CheckCost] AbilityID '%s' 在 AbilityTable 无对应行，拒绝激活"), *AbilityID.ToString());
		return false;
	}
	
	// 根因：引擎 CheckCost 只会拿 CostGE 的 CDO 新建 Spec，不会带 SetByCaller。
	// Data.Cost 未设置时 GetMagnitude 会报错并按 0 处理，于是耗能检查恒通过。
	// 这里注入与 ApplyCost 完全相同的一份数值，再按引擎语义检查。
	FGameplayEffectSpec CostSpec(CostGE, MakeEffectContext(Handle, ActorInfo), GetAbilityLevel(Handle, ActorInfo));
	CostSpec.SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(FName("Data.Cost")), -Row.EnergyCost);
	CostSpec.CalculateModifierMagnitudes();
	
	// 与 FActiveGameplayEffectsContainer::CanApplyAttributeModifiers 同语义：
	// 只检查 Additive（AddBase 的兼容枚举值 = 0），Current + Delta < 0 即付不起。
	for (int32 Index = 0; Index < CostGE->Modifiers.Num() && Index < CostSpec.Modifiers.Num(); Index++)
	{
		const FGameplayModifierInfo& ModDef = CostGE->Modifiers[Index];
		if (ModDef.ModifierOp != EGameplayModOp::AddBase || ModDef.Attribute.IsValid() == false)
		{
			continue;
		}
		
		const float Current = ASC->GetNumericAttribute(ModDef.Attribute);
		const float Delta = CostSpec.Modifiers[Index].GetEvaluatedMagnitude();
		if (Current + Delta < 0.0f)
		{
			if (OptionalRelevantTags)
			{
				const FGameplayTag& FailCostTag = UAbilitySystemGlobals::Get().ActivateFailCostTag;
				if (FailCostTag.IsValid())
				{
					OptionalRelevantTags->AddTag(FailCostTag);
				}
			}
			
			UE_LOG(LogArcane, Verbose, TEXT("[CheckCost] %s：%s 能量不足（当前 %.1f，需要 %.1f）"),
				*GetNameSafe(ActorInfo->OwnerActor.Get()), *AbilityID.ToString(), Current, -Delta);
			return false;
		}
	}
	
	return true;
}
