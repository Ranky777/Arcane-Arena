// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneGameplayAbility_Melee.h"

#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Combat/ArcaneAbilityAvatar.h"

void UArcaneGameplayAbility_Melee::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                                   const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                                   const FGameplayEventData* TriggerEventData)
{
	if (ActorInfo == nullptr)
	{
		return;
	}
	
	if (!ActorInfo->AvatarActor.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	IArcaneAbilityAvatar* Avatar = Cast<IArcaneAbilityAvatar>(ActorInfo->AvatarActor.Get());
	if (Avatar == nullptr)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	if (AttackMode == EArcaneMeleeAttackMode::Combo)
	{
		// 原 GA_PrimaryAttack：起手后立刻 End，不 Commit。
		Avatar->ExecutePrimaryAttack(AbilityID);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}
	else
	{
		// 原 GA_SecondaryAttack：先起手，松手才 Commit + Finish。
		Avatar->ExecuteSecondaryAttack(AbilityID);
		
		UAbilityTask_WaitInputRelease* ReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, false);
		
		if (ReleaseTask == nullptr)
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
 
		ReleaseTask->OnRelease.AddDynamic(this, &UArcaneGameplayAbility_Melee::HandleInputReleased);
		ReleaseTask->ReadyForActivation();
	}
}

void UArcaneGameplayAbility_Melee::HandleInputReleased(float TimeHeld)
{
	if (!IsActive() || CurrentActorInfo == nullptr)
	{
		return;
	}
	
	CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo);
	
	if (IArcaneAbilityAvatar* Avatar = Cast<IArcaneAbilityAvatar>(CurrentActorInfo->AvatarActor.Get()))
	{
		Avatar->ExecuteFinishSecondaryAttack();
	}
	
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
