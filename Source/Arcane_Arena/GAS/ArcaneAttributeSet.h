// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "ArcaneAttributeSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * 
 */
UCLASS()
class ARCANE_ARENA_API UArcaneAttributeSet : public UAttributeSet
{
	GENERATED_BODY()
	
public:
	UArcaneAttributeSet();
	
	// --- 生命数值 ---
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Arcane|Vital")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UArcaneAttributeSet, Health)
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Arcane|Vital")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UArcaneAttributeSet, MaxHealth)
	
	// --- 能量（技能资源，见玩法文档 §4.2）---
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Energy, Category = "Arcane|Vital")
	FGameplayAttributeData Energy;
	ATTRIBUTE_ACCESSORS(UArcaneAttributeSet, Energy)
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxEnergy, Category = "Arcane|Vital")
	FGameplayAttributeData MaxEnergy;
	ATTRIBUTE_ACCESSORS(UArcaneAttributeSet, MaxEnergy)
	
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Arcane|Vital")
	FGameplayAttributeData EnergyRegen;
	ATTRIBUTE_ACCESSORS(UArcaneAttributeSet, EnergyRegen)
	
	// --- 移动（Buff/减速改写目标）---
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Arcane|Movement")
	FGameplayAttributeData MoveSpeed;
	ATTRIBUTE_ACCESSORS(UArcaneAttributeSet, MoveSpeed)
	
protected:
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);
	
	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
	
	UFUNCTION()
	void OnRep_Energy(const FGameplayAttributeData& OldValue);
	
	UFUNCTION()
	void OnRep_MaxEnergy(const FGameplayAttributeData& OldValue);
};
