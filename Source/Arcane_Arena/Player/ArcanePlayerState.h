// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "ArcanePlayerState.generated.h"

class UArcaneAttributeSet;
class UArcaneAbilitySystemComponent;
/**
 * 
 */
UCLASS()
class ARCANE_ARENA_API AArcanePlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()
	
public:
	AArcanePlayerState();
	
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UArcaneAbilitySystemComponent* GetArcaneASC() const { return ArcaneASC; }
	UArcaneAttributeSet* GetArcaneAttributeSet() const { return AttributeSet; }
	
protected:
	UPROPERTY(VisibleAnywhere, Category = "Arcane|GAS")
	TObjectPtr<UArcaneAbilitySystemComponent> ArcaneASC;
	
	UPROPERTY()
	TObjectPtr<UArcaneAttributeSet> AttributeSet;
};
