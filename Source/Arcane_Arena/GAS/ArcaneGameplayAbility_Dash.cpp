// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneGameplayAbility_Dash.h"

#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "GameFramework/Character.h"

void UArcaneGameplayAbility_Dash::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                                  const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                                  const FGameplayEventData* TriggerEventData)
{
	if (ActorInfo == nullptr)
	{
		return;
	}
	
	AActor* Avatar = ActorInfo->AvatarActor.Get();
	if (Avatar == nullptr)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	CommitAbility(Handle, ActorInfo, ActivationInfo);
	
	if (ACharacter* Character = Cast<ACharacter>(Avatar))
	{
		if (USkeletalMeshComponent* Mesh = Character->GetMesh())
		{
			if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
			{
				AnimInstance->Montage_Stop(MontageStopBlendOut, nullptr);
			}
		}
	}
	
	const FVector Velocity = Avatar->GetVelocity();
	const FVector Direction = Velocity.Size() > 0.01f ? Velocity.GetSafeNormal() : Avatar->GetActorForwardVector();
	
	UAbilityTask_ApplyRootMotionConstantForce* ForceTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(this, NAME_None, Direction, 
		DashStrength, DashDuration, false, nullptr, ERootMotionFinishVelocityMode::MaintainLastRootMotionVelocity,
		FVector::ZeroVector, 0.0f, false);
	
	if (ForceTask == nullptr)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	ForceTask->OnFinish.AddDynamic(this, &UArcaneGameplayAbility_Dash::OnDashFinished);
	ForceTask->ReadyForActivation();
	
	// 原蓝图在 Force 节点后并联了一个 0.15s 的 WaitDelay 兜底。
	// 这里保留同样的双保险：两个任务都是 0.15s，任一结束都走 OnDashFinished。
	UAbilityTask_WaitDelay* DelayTask =
		UAbilityTask_WaitDelay::WaitDelay(this, DashDuration);
 
	if (DelayTask != nullptr)
	{
		DelayTask->OnFinish.AddDynamic(this, &UArcaneGameplayAbility_Dash::OnDashFinished);
		DelayTask->ReadyForActivation();
	}
}

void UArcaneGameplayAbility_Dash::OnDashFinished()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}
