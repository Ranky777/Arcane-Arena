// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArcaneGameplayAbility.h"
#include "ArcaneGameplayAbility_Dash.generated.h"

/**
 * 冲刺能力：委托根部保持不变，只把原 GA_Dash 蓝图的执行链搬进 C++。
 * 蓝图子类只填 AbilityID / InputID / Cooldown GE / Cost GE，EventGraph 留空。
 */
UCLASS()
class ARCANE_ARENA_API UArcaneGameplayAbility_Dash : public UArcaneGameplayAbility
{
	GENERATED_BODY()

public:
	// 根运动强度
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arcane|Dash", meta = (ClampMin = "0.0"))
	float DashStrength = 3000.0f;
 
	// 根运动持续时间（秒）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arcane|Dash", meta = (ClampMin = "0.01"))
	float DashDuration = 0.15f;
 
	// 起手时停止当前 Montage 的混合时间（秒）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arcane|Dash", meta = (ClampMin = "0.0"))
	float MontageStopBlendOut = 0.1f;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	
	UFUNCTION()
	void OnDashFinished();
};
