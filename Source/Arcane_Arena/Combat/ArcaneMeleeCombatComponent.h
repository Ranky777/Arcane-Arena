// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ArcaneMeleeCombatComponent.generated.h"

class UGameplayEffect;
/**
 * 近战结算组件。
 * 负责：一次 trace、读一次表、逐命中构造 Payload、调用 IArcaneDamageable、
 * 再走 ApplyImpact（状态/Cue/受击反应/顿帧）与 CameraShake。
 *
 * 刻意不做 actor 去重：保持原蓝图“一个 MultiSphereTrace 命中几次就结算几次”的行为。
 */
UCLASS(ClassGroup=(Arcane), meta=(BlueprintSpawnableComponent))
class ARCANE_ARENA_API UArcaneMeleeCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UArcaneMeleeCombatComponent();

public:
	UFUNCTION(BlueprintCallable, Category = "Arcane|Combat")
	void SetCurrentAttackAbilityID(FName AbilityID);
	
	UFUNCTION(BlueprintPure, Category = "Arcane|Combat")
	FName GetCurrentAttackAbilityID() const { return CurrentAbilityID; }
	
	// 由动画 Notify 的 DoAttackTrace 转发进来
	UFUNCTION(BlueprintCallable, Category = "Arcane|Combat")
	void PerformAttackTrace(FName SourceBone);
	
	// 通用时长 GE（GE_Status），在角色 BP 的组件默认值里指定
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arcane|Combat")
	TSubclassOf<UGameplayEffect> StatusEffectClass;
	
	// 原蓝图硬编码的 BP_CameraShake_Hit_Enemy，在组件默认值里指定
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arcane|Combat")
	TSubclassOf<UCameraShakeBase> HitCameraShakeClass;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arcane|Combat", meta = (ClampMin = "0.0"))
	float CameraShakeInnerRadius = 500.0f;
 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arcane|Combat", meta = (ClampMin = "0.0"))
	float CameraShakeOuterRadius = 1000.0f;
	
private:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Arcane|Combat", meta = (AllowPrivateAccess = "true"))
	FName CurrentAbilityID = NAME_None;
};
