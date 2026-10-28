// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArcaneGameplayAbility.h"
#include "ArcaneGameplayAbility_Melee.generated.h"

UENUM(BlueprintType)
enum class EArcaneMeleeAttackMode : uint8
{
	Combo UMETA(DisplayName = "Combo"),
	Charged UMETA(DisplayName = "Charged")
};

/**
 * 近战能力基类。
 * Combo   = 左键连击：激活即起手，不 Commit（保持原 GA_PrimaryAttack 行为）
 * Charged = 右键蓄力：激活起手，松手才 Commit + Finish（保持原 GA_SecondaryAttack 行为）
 *
 * 蓝图子类只填 AbilityID / InputID / Cooldown GE / Cost GE + AttackMode，EventGraph 留空。
 */
UCLASS(Blueprintable)
class ARCANE_ARENA_API UArcaneGameplayAbility_Melee : public UArcaneGameplayAbility
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arcane|Melee")
	EArcaneMeleeAttackMode AttackMode = EArcaneMeleeAttackMode::Combo;
	
protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	
	UFUNCTION()
	void HandleInputReleased(float TimeHeld);
};
