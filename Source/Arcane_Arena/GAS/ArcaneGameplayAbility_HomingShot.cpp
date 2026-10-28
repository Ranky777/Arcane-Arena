// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneGameplayAbility_HomingShot.h"

#include "Arcane_Arena.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "AbilityTasks/AbilityTask_PerformTargeting.h"
#include "Character/ArcaneCharacter.h"
#include "Combat/ArcaneCombatData.h"
#include "Combat/ArcaneCombatDataLibrary.h"
#include "Combat/ArcaneCombatFunctionLibrary.h"
#include "Combat/ArcaneTargetingSelectionTask_Lock.h"
#include "Player/ArcanePlayerController.h"
#include "TargetingSystem/TargetingPreset.h"

void UArcaneGameplayAbility_HomingShot::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                                        const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                                        const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	
	// 能走到这里说明 CanActivateAbility 已过冷却与耗能门禁 —— 选敌查询不会白跑
	
	if (!ensureMsgf(TargetingPreset != nullptr, TEXT("[%s] 未配置 TargetingPreset"), *GetNameSafe(this)))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	// 按下：只开预览，不选敌、不 Commit、不发射
	ShowRangePreview();
	
	// bTestAlreadyReleased=true：快速点按时，松手事件可能早于 Task 创建，直接当已释放处理
	UAbilityTask_WaitInputRelease* ReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	if (ReleaseTask == nullptr)
	{
		HideRangePreview();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	ReleaseTask->OnRelease.AddDynamic(this, &UArcaneGameplayAbility_HomingShot::HandleInputReleased);
	ReleaseTask->ReadyForActivation();
}

void UArcaneGameplayAbility_HomingShot::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// 兜底：死亡、回合锁、预测被服务器拒绝、任何取消路径都必须收圈
	HideRangePreview();
	
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UArcaneGameplayAbility_HomingShot::HandleInputReleased(float TimeHeld)
{
	UE_LOG(LogArcane, Verbose, TEXT("[%s] 松开（%.2fs），开始选敌"), *AbilityID.ToString(), TimeHeld);
 
	// 松开：先收 UI，再走原来的 选敌 → Commit → 发射 链路
	HideRangePreview();
	PerformTargetingRequest();
}

void UArcaneGameplayAbility_HomingShot::OnTargetReady(FTargetingRequestHandle TargetingHandle)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	IArcaneAbilityAvatar* Avatar = ActorInfo ? Cast<IArcaneAbilityAvatar>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (Avatar == nullptr)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	
	// 先付钱再打人：CommitAbility 内部会重查一遍冷却/耗能（引擎明说 commit 可能失败），
	// 放在发射之前调，就不会出现"弹已生成、能量没扣、冷却没挂"的白嫖窗口
	if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	
	// Sort 任务已把最近的合法目标排到第 0 位；选不到就是 nullptr（服务器仍可兜底）
	AActor* Proposed = UArcaneCombatFunctionLibrary::GetFirstTargetingResult(GetWorld(), TargetingHandle);
	const FVector AimPoint = UArcaneCombatFunctionLibrary::GetTargetingAimPoint(TargetingHandle);	
	
	// 只调契约，不做任何"该不该发射/目标合不合法"的判断——那是角色权威层的事
	Avatar->ExecuteHomingAttack(AbilityID, Proposed, AimPoint);
	
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UArcaneGameplayAbility_HomingShot::PerformTargetingRequest()
{
	UAbilityTask_PerformTargeting* Task = UAbilityTask_PerformTargeting::PerformTargetingRequest(
		this, TargetingPreset, /*bAllowAsync=*/false);
 
	if (Task == nullptr)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
 
	Task->OnTargetReady.AddDynamic(this, &UArcaneGameplayAbility_HomingShot::OnTargetReady);
	Task->ReadyForActivation();
}

void UArcaneGameplayAbility_HomingShot::ShowRangePreview()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (ActorInfo == nullptr || ActorInfo->IsLocallyControlled() == false)
	{
		return; // 服务器代远程玩家跑的那一份、Bot 那一份都不显示
	}
 
	APlayerController* PC = ActorInfo->PlayerController.Get();
	IArcaneRangePreviewClient* PreviewClientInterface = Cast<IArcaneRangePreviewClient>(PC);
	
	if (PreviewClientInterface == nullptr)
	{
		return;
	}
 
	bool bFound = false;
	const FArcaneAbilityRow Row = UArcaneCombatDataLibrary::GetAbilityRow(AbilityID, bFound);
	if (bFound == false || Row.TargetingMode != EArcaneTargetingMode::Projectile)
	{
		return;
	}
 
	PreviewClient = PC;

	// 内圈 = 选敌窗口（Preset 的 Lock 半径），不是弹体碰撞半径
	const float InnerRadius = ResolvePreviewInnerRadius();
	if (InnerRadius <= 0.0f)
	{
		UE_LOG(LogArcane, Verbose, TEXT("[%s] TargetingPreset 里没有 Lock 选敌任务，内圈不显示"), *AbilityID.ToString());
	}

	PreviewClientInterface->ShowAbilityRangePreview(ActorInfo->AvatarActor.Get(), Row.Range, InnerRadius);
}

float UArcaneGameplayAbility_HomingShot::ResolvePreviewInnerRadius() const
{
	if (TargetingPreset == nullptr)
	{
		return 0.0f;
	}

	const FTargetingTaskSet* TaskSet = TargetingPreset->GetTargetingTaskSet();
	if (TaskSet == nullptr)
	{
		return 0.0f;
	}

	for (const TObjectPtr<UTargetingTask>& Task : TaskSet->Tasks)
	{
		if (const UArcaneTargetingSelectionTask_Lock* LockTask = Cast<UArcaneTargetingSelectionTask_Lock>(Task.Get()))
		{
			return LockTask->GetLockRadius();
		}
	}

	return 0.0f;
}

void UArcaneGameplayAbility_HomingShot::HideRangePreview()
{
	if (UObject* ClientObject = PreviewClient.Get())
	{
		if (IArcaneRangePreviewClient* Preview =
			Cast<IArcaneRangePreviewClient>(ClientObject))
		{
			Preview->HideAbilityRangePreview();
		}
	}
	
	PreviewClient.Reset();
}
