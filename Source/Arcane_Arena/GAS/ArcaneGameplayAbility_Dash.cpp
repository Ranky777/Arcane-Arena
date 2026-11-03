// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneGameplayAbility_Dash.h"

#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Character/ArcaneCharacterMovementComponent.h"
#include "GameFramework/Character.h"

void UArcaneGameplayAbility_Dash::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                                  const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                                  const FGameplayEventData* TriggerEventData)
{
	if (ActorInfo == nullptr)
	{
		return;
	}
	
	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	if (AvatarActor == nullptr)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	// 先 Commit 再动：Commit 失败（冷却没到/能量不够/预测被服务器拒绝）就不该有位移
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	ACharacter* Character = Cast<ACharacter>(AvatarActor);
	UArcaneCharacterMovementComponent* CMC = Character
		? Cast<UArcaneCharacterMovementComponent>(Character->GetCharacterMovement())
		: nullptr;
	if (CMC == nullptr)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	if (USkeletalMeshComponent* Mesh = Character->GetMesh())
	{
		if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
		{
			AnimInstance->Montage_Stop(MontageStopBlendOut, nullptr);
		}
	}
	
	if (!CMC->StartDash(DashStrength, DashDuration))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	UAbilityTask_WaitDelay* DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, DashDuration);
	if (DelayTask == nullptr)
	{
		CMC->StopDash();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	DelayTask->OnFinish.AddDynamic(this, &UArcaneGameplayAbility_Dash::OnDashFinished);
	DelayTask->ReadyForActivation();
}

void UArcaneGameplayAbility_Dash::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// 被眩晕/击退/死亡/回合锁取消时，能力结束但 CMC 的冲刺状态必须一起清掉
	if (bWasCancelled && ActorInfo != nullptr)
	{
		if (ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get()))
		{
			if (UArcaneCharacterMovementComponent* CMC = Cast<UArcaneCharacterMovementComponent>(Character->GetCharacterMovement()))
			{
				CMC->StopDash();
			}
		}
	}
	
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UArcaneGameplayAbility_Dash::OnDashFinished()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}
