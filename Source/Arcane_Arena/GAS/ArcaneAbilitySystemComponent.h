// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "ArcaneAbilitySystemComponent.generated.h"


enum class EArcaneAbilityInputID : uint8;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnArcaneAbilityCommitted);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ARCANE_ARENA_API UArcaneAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UArcaneAbilitySystemComponent();
	
	// 供HUD使用，按照InputID查询技能冷却，返回flase = 不在冷却中
	UFUNCTION(BlueprintCallable, Category = "Arcane|GAS")
	bool GetCooldownTimes(EArcaneAbilityInputID InputID, float& Remaining, float& Duration) const;
	
	UPROPERTY(BlueprintAssignable, Category = "Arcane|GAS")
	FOnArcaneAbilityCommitted OnAbilityCommitted;
	
protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	
	void HandleAbilityCommitted(UGameplayAbility* Ability);
};
