// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneCombatFunctionLibrary.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "ArcaneCombatData.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "Kismet/GameplayStatics.h"
#include "Player/ArcanePlayerState.h"

float UArcaneCombatFunctionLibrary::ApplyArcaneDamage(AActor* SourceActor, AActor* TargetActor,
                                                      TSubclassOf<UGameplayEffect> DamageGameplayEffectClass, float Damage, const FHitResult& HitResult)
{
	const IAbilitySystemInterface* TargetASI = Cast<IAbilitySystemInterface>(TargetActor);
	UAbilitySystemComponent* TargetASC = TargetASI != nullptr ? TargetASI->GetAbilitySystemComponent() : nullptr;

	if (TargetASC == nullptr || DamageGameplayEffectClass == nullptr || Damage <= 0.0f)
	{
		return 0.0f;
	}

	// 权威门槛：伤害只在服务器结算。客户端预测激活时调这里会直接返回 0。
	if (TargetActor == nullptr || !TargetActor->HasAuthority())
	{
		return 0.0f;
	}
	
	// 友军伤害门禁：同队（且双方都有合法阵营）直接免伤
	const int32 SourceTeam = GetActorTeamID(SourceActor);
	const int32 TargetTeam = GetActorTeamID(TargetActor);
	if (SourceTeam >= 0 && SourceTeam == TargetTeam)
	{
		return 0.0f;
	}

	// 来源 ASC 可为空（纯环境伤害如熔岩），此时用目标 ASC 构建 Spec 上下文
	const IAbilitySystemInterface* SourceASI = Cast<IAbilitySystemInterface>(SourceActor);
	UAbilitySystemComponent* SourceASC = SourceASI != nullptr ? SourceASI->GetAbilitySystemComponent() : TargetASC;

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();

	// 命中信息写入 Context（AddHitResult 在 ContextHandle 上，不在 Spec 上）；
	// Spec 复制 Context → GameplayCue 参数的 Location/Normal 自动取自这里的 HitResult
	Context.AddHitResult(HitResult);

	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(DamageGameplayEffectClass, 1.0f, Context);
	if (!SpecHandle.IsValid())
	{
		return 0.0f;
	}

	// ⚠ GE_Damage 的 Health 修改器是 AddBase（加法），SetByCaller 必须注入负值才是伤害；
	// 注入正数会变成治疗（血量越打越高）
	SpecHandle.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(FName("Data.Damage")), -Damage);
	SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);

	return Damage;
}

void UArcaneCombatFunctionLibrary::ApplyImpact(AActor* SourceActor, AActor* TargetActor,
	TSubclassOf<UGameplayEffect> StatusEffectClass, const FArcaneImpactRow& ImpactRow, const FHitResult& HitResult)
{
	if (TargetActor == nullptr)
	{
		return;
	}
	const bool bServer = SourceActor != nullptr && SourceActor->HasAuthority();
	
	if (IAbilitySystemInterface* TargetASI = Cast<IAbilitySystemInterface>(TargetActor))
	{
		if (UAbilitySystemComponent* TargetASC = TargetASI->GetAbilitySystemComponent())
		{
			// 1. 状态标签（服务器）：GE_Status 容器 + DynamicGrantedTags + SetByCaller Data.Duration
			if (bServer && StatusEffectClass && ImpactRow.StatusTags.Num() > 0 && ImpactRow.StatusDuration > 0.0f)
			{
				const UGameplayEffect* GEDef = GetDefault<UGameplayEffect>(StatusEffectClass);
				FGameplayEffectSpec Spec(GEDef, TargetASC->MakeEffectContext(), 1.0f);
				Spec.DynamicGrantedTags.AppendTags(ImpactRow.StatusTags);
				Spec.SetSetByCallerMagnitude(
					FGameplayTag::RequestGameplayTag(FName("Data.Duration")), ImpactRow.StatusDuration);
				TargetASC->ApplyGameplayEffectSpecToSelf(Spec);
			}
			
			// 2. GameplayCue（本次表列留空，预留）
			FGameplayCueParameters Params;
			Params.Location = HitResult.ImpactPoint;
			Params.Normal = HitResult.ImpactNormal;
			Params.Instigator = SourceActor;
			Params.EffectContext = TargetASC->MakeEffectContext();
			for (const FGameplayTag& CueTag : ImpactRow.GameplayCueTags)
			{
				TargetASC->ExecuteGameplayCue(CueTag, Params);
			}
			
			// 3. 受击反应蒙太奇（服务器发起，经角色复制；BP 实现 PlayHitReaction，见蓝图步骤 4）
			if (bServer && ImpactRow.HitReactionTag.IsValid())
			{
				if (UFunction* Fn = TargetActor->FindFunction("PlayHitReaction"))
				{
					struct FHitReactionParams
					{
						FGameplayTag Tag;
					};
					
					FHitReactionParams Param;
					Param.Tag = ImpactRow.HitReactionTag;
					TargetActor->ProcessEvent(Fn, &Param);
				}
			}
			
			// 4. 命中顿帧（服务器）。⚠️ TimerManager 走游戏时间，会被 Dilation 拖慢，
			//    所以恢复时长要乘 Dilation 折算，保证真实暂停≈HitPause 秒
			if (bServer && ImpactRow.HitPause > 0.0f && SourceActor)
			{
				if (UWorld* World = SourceActor->GetWorld())
				{
					constexpr float Dilation = 0.1f;
					UGameplayStatics::SetGlobalTimeDilation(World, Dilation);
					
					FTimerHandle Handle;
					World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(World, [World]()
					{
						if (World->IsValidLowLevel())
						{
							UGameplayStatics::SetGlobalTimeDilation(World, 1.0f);
						}
					}), ImpactRow.HitPause * Dilation, false);
				}
			}
		}
	}
}

int32 UArcaneCombatFunctionLibrary::GetActorTeamID(const AActor* Actor)
{
	if (Actor == nullptr)
	{
		return -1;
	}
	
	if (const APawn* Pawn = Cast<APawn>(Actor))
	{
		if (const AArcanePlayerState* PS = Pawn->GetPlayerState<AArcanePlayerState>())
		{
			return PS->TeamID;
		}
	}
	
	if (const AArcanePlayerState* PS = Cast<AArcanePlayerState>(Actor))
	{
		return PS->TeamID; // 直接传了 PlayerState 的情况
	}
	
	return -1;
}
