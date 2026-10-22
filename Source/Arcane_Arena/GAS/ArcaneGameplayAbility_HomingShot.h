// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArcaneGameplayAbility.h"
#include "Types/TargetingSystemTypes.h"
#include "ArcaneGameplayAbility_HomingShot.generated.h"

class AArcanePlayerController;
class UTargetingPreset;
/**
 * 远程锁定技基类：选敌交给 TargetingSystem 的 Preset（数据资产），
 * 发射与裁决一律走 AArcaneCharacter 的攻击契约。
 * 子类（BP）只需填：AbilityID + InputID + TargetingPreset + GE_Cooldown + GE_Cost，图表留空。
 */
UCLASS()
class ARCANE_ARENA_API UArcaneGameplayAbility_HomingShot : public UArcaneGameplayAbility
{
	GENERATED_BODY()
	
public:
	// 选敌管线（Selection → Filter → Filter → Sort）全部配置在这个 DataAsset 里
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arcane|Targeting")
	TObjectPtr<UTargetingPreset> TargetingPreset;
	
protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	
	UFUNCTION()
	void HandleInputReleased(float TimeHeld);
	
private:
	UFUNCTION()
	void OnTargetReady(FTargetingRequestHandle TargetingHandle);
	
	void PerformTargetingRequest();
	void ShowRangePreview();
	void HideRangePreview();

	// 预览内圈半径：读 TargetingPreset 里 Lock 选敌任务的窗口半径。
	// 注意不是 AbilityTable 的 Radius —— 那个只用于弹体碰撞球。
	float ResolvePreviewInnerRadius() const;
	
	TWeakObjectPtr<AArcanePlayerController> PreviewPC;
};
