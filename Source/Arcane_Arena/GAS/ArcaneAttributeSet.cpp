// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneAttributeSet.h"

#include "Net/UnrealNetwork.h"

UArcaneAttributeSet::UArcaneAttributeSet()
{
	// 默认值全部视为 [PLACEHOLDER]，playtest 后回填
	InitHealth(100.f);
	InitMaxHealth(100.f);
	InitEnergy(50.f);
	InitMaxEnergy(100.f);
	InitEnergyRegen(8.f);
	InitMoveSpeed(600.f);
}

void UArcaneAttributeSet::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME_CONDITION_NOTIFY(UArcaneAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArcaneAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArcaneAttributeSet, Energy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArcaneAttributeSet, MaxEnergy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME(UArcaneAttributeSet, EnergyRegen);
	DOREPLIFETIME(UArcaneAttributeSet, MoveSpeed);
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
