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
	
	UFUNCTION(BlueprintCallable)
	UArcaneAbilitySystemComponent* GetArcaneASC() const { return ArcaneASC; }
	
	UArcaneAttributeSet* GetArcaneAttributeSet() const { return AttributeSet; }
	
	// 3v3 队伍 ID（0/1），GameMode PostLogin 交替分配
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Team, Category = "Arcane|Match")
	int32 TeamID = 0;
	
protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	UFUNCTION()
	void OnRep_Team();
	
	UPROPERTY(VisibleAnywhere, Category = "Arcane|GAS")
	TObjectPtr<UArcaneAbilitySystemComponent> ArcaneASC;
	
	UPROPERTY()
	TObjectPtr<UArcaneAttributeSet> AttributeSet;
};
