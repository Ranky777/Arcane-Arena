// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneGameplayAbility_Shield.h"

#include "AbilitySystemComponent.h"
#include "ArcaneAttributeSet.h"
#include "Arcane_Arena.h"

UArcaneGameplayAbility_Shield::UArcaneGameplayAbility_Shield()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UArcaneGameplayAbility_Shield::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (ShieldGrantEffectClass == nullptr || ShieldBuffEffectClass == nullptr)
	{
		UE_LOG(LogArcane, Error, TEXT("[GA_Shield] Grant/Buff GE 未配置，拒绝激活"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC == nullptr)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	// 重施时先拆旧 buff：旧 buff 的移除回调会清空属性，
	// 随后再授予新盾，保证旧回调永远清不到新盾。
	RemoveActiveShieldBuff(ASC);
	
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(this);
	
	const FGameplayEffectSpecHandle GrantSpec = ASC->MakeOutgoingSpec(ShieldGrantEffectClass, GetAbilityLevel(), Context);
	if (GrantSpec.IsValid())
	{
		ASC->ApplyGameplayEffectSpecToSelf(*GrantSpec.Data.Get());
	}
	
	const FGameplayEffectSpecHandle BuffSpec = ASC->MakeOutgoingSpec(ShieldBuffEffectClass, GetAbilityLevel(), Context);
	if (BuffSpec.IsValid())
	{
		ActiveShieldBuffHandle = ASC->ApplyGameplayEffectSpecToSelf(*BuffSpec.Data.Get());
		
		if (FOnActiveGameplayEffectRemoved_Info* RemovalDelegate = 
			ASC->OnGameplayEffectRemoved_InfoDelegate(ActiveShieldBuffHandle))
		{
			RemovalDelegate->AddUObject(this, &UArcaneGameplayAbility_Shield::HandleShieldBuffRemoved);
		}
	}
	
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UArcaneGameplayAbility_Shield::OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilitySpec& Spec)
{
	if (ActorInfo != nullptr)
	{
		if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
		{
			RemoveActiveShieldBuff(ASC);
			ClearShieldAttributes(ASC);
		}
	}
	
	Super::OnRemoveAbility(ActorInfo, Spec);
}

void UArcaneGameplayAbility_Shield::HandleShieldBuffRemoved(const FGameplayEffectRemovalInfo& RemovalInfo)
{
	// 重施时旧 buff 的移除回调可能晚于新 buff 的授予：
	// 只要回调对应的不是当前 buff，就不能清掉新盾。
	if (RemovalInfo.ActiveEffect != nullptr
		&& ActiveShieldBuffHandle.IsValid()
		&& RemovalInfo.ActiveEffect->Handle != ActiveShieldBuffHandle)
	{
		return;
	}
	
	ActiveShieldBuffHandle.Invalidate();
	
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ClearShieldAttributes(ASC);
	}
	
	UE_LOG(LogArcane, Log, TEXT("[GA_Shield] buff 移除（premature=%d），护盾/减伤清零"),
		RemovalInfo.bPrematureRemoval ? 1 : 0);
}

void UArcaneGameplayAbility_Shield::RemoveActiveShieldBuff(UAbilitySystemComponent* ASC)
{
	if (ASC == nullptr || !ActiveShieldBuffHandle.IsValid())
	{
		return;
	}
	
	// RemoveActiveGameplayEffect 会同步触发 HandleShieldBuffRemoved（负责清属性）
	ASC->RemoveActiveGameplayEffect(ActiveShieldBuffHandle);
	ActiveShieldBuffHandle.Invalidate();
}

void UArcaneGameplayAbility_Shield::ClearShieldAttributes(UAbilitySystemComponent* ASC) const
{
	// 只由权威端写 BaseValue；客户端靠 AttributeSet 复制收敛，
	// 避免在非自治 ASC（观战/其他玩家）上写 base 造成预测噪声
	if (ASC == nullptr || !ASC->IsOwnerActorAuthoritative())
	{
		return;
	}
	
	ASC->SetNumericAttributeBase(UArcaneAttributeSet::GetShieldAttribute(), 0.0f);
	ASC->SetNumericAttributeBase(UArcaneAttributeSet::GetDamageReductionAttribute(), 0.0f);
}
