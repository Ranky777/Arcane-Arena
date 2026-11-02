// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArcaneGameplayAbility.h"
#include "ArcaneGameplayAbility_Shield.generated.h"

/**
 * 
 */
UCLASS()
class ARCANE_ARENA_API UArcaneGameplayAbility_Shield : public UArcaneGameplayAbility
{
	GENERATED_BODY()
	
public:
	UArcaneGameplayAbility_Shield();
	
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	
	virtual void OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	
protected:
	UPROPERTY(EditDefaultsOnly, Category = "Arcane|Shield")
	TSubclassOf<UGameplayEffect> ShieldGrantEffectClass;
	
	UPROPERTY(EditDefaultsOnly, Category = "Arcane|Shield")
	TSubclassOf<UGameplayEffect> ShieldBuffEffectClass;
	
	void HandleShieldBuffRemoved(const FGameplayEffectRemovalInfo& RemovalInfo);
	void RemoveActiveShieldBuff(UAbilitySystemComponent* ASC);
	void ClearShieldAttributes(UAbilitySystemComponent* ASC) const;
	
	FActiveGameplayEffectHandle ActiveShieldBuffHandle;
};
