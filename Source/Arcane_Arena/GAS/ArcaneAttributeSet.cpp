// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UArcaneAttributeSet::UArcaneAttributeSet()
{
	// 默认值全部视为 [PLACEHOLDER]，playtest 后回填
	InitHealth(100.f);
	InitMaxHealth(100.f);
	InitEnergy(100.f); // 初始能量=满值（与 MaxEnergy 一致），否则开局只有 50
	InitMaxEnergy(100.f);
	InitEnergyRegen(8.f);
	InitMoveSpeed(600.f);
	InitShield(0.0f);
	InitMaxShield(100.0f);
	InitDamageReduction(0.0f);
}

void UArcaneAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
	}
	else if (Data.EvaluatedData.Attribute == GetEnergyAttribute())
	{
		SetEnergy(FMath::Clamp(GetEnergy(), 0.f, GetMaxEnergy()));
	}
	else if (Data.EvaluatedData.Attribute == GetShieldAttribute())
	{
		SetShield(FMath::Clamp(GetShield(), 0.f, GetMaxShield()));
		
		// 护盾被打空：提前结束 Status.Shield 状态。
		// 移除 GE_ShieldBuff 会连锁完成三件事：
		//   1. GA_Shield 的移除回调清零 Shield / DamageReduction；
		//   2. GE 上的 GameplayCue.Shield 收到 Removed，护盾特效提前淡出；
		//   3. 剩余时长的减伤一并结束（盾破了不应该还留着减伤）。
		if (GetShield() <= 0.0f && Data.EvaluatedData.Magnitude < 0.0f)
		{
			if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
			{
				FGameplayTagContainer ShieldBuffTags;
				ShieldBuffTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.Shield")));
				ASC->RemoveActiveEffectsWithGrantedTags(ShieldBuffTags);
			}
		}
	}
}

void UArcaneAttributeSet::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME_CONDITION_NOTIFY(UArcaneAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArcaneAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArcaneAttributeSet, Energy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArcaneAttributeSet, MaxEnergy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArcaneAttributeSet, EnergyRegen, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArcaneAttributeSet, MoveSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArcaneAttributeSet, Shield, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArcaneAttributeSet, MaxShield, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArcaneAttributeSet, DamageReduction, COND_None, REPNOTIFY_Always);
}

void UArcaneAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArcaneAttributeSet, Health, OldValue);
}

void UArcaneAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArcaneAttributeSet, MaxHealth, OldValue);
}

void UArcaneAttributeSet::OnRep_Energy(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArcaneAttributeSet, Energy, OldValue);
}

void UArcaneAttributeSet::OnRep_MaxEnergy(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArcaneAttributeSet, MaxEnergy, OldValue);
}

void UArcaneAttributeSet::OnRep_EnergyRegen(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArcaneAttributeSet, EnergyRegen, OldValue);
}

void UArcaneAttributeSet::OnRep_MoveSpeed(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArcaneAttributeSet, MoveSpeed, OldValue);
}

void UArcaneAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	
	if (Attribute == GetDamageReductionAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, 0.9f);
	}
}

void UArcaneAttributeSet::OnRep_Shield(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArcaneAttributeSet, Shield, OldValue);
}

void UArcaneAttributeSet::OnRep_MaxShield(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArcaneAttributeSet, MaxShield, OldValue);
}

void UArcaneAttributeSet::OnRep_DamageReduction(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArcaneAttributeSet, DamageReduction, OldValue);
}
