// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneCombatFunctionLibrary.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Arcane_Arena.h"
#include "ArcaneCombatData.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "Character/ArcaneCharacter.h"
#include "Core/ArcaneTeamUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Player/ArcanePlayerState.h"
#include "TargetingSystem/TargetingSubsystem.h"


// 瞄准点抬到胸部：GetActorLocation 是胶囊中心，打脚底会命中率偏低
static constexpr float ArcaneLockOnAimZ = 60.0f;

float UArcaneCombatFunctionLibrary::ApplyArcaneDamage(AActor* SourceActor, AActor* TargetActor,
                                                      TSubclassOf<UGameplayEffect> DamageGameplayEffectClass, float Damage, const FHitResult& HitResult)
{
	const IAbilitySystemInterface* TargetASI = Cast<IAbilitySystemInterface>(TargetActor);
	UAbilitySystemComponent* TargetASC = TargetASI != nullptr ? TargetASI->GetAbilitySystemComponent() : nullptr;

	if (TargetASC == nullptr || DamageGameplayEffectClass == nullptr || Damage <= 0.0f)
	{
		UE_LOG(LogArcane, Warning, TEXT("[Damage] 被拦：Target=%s ASC=%s GE=%s Damage=%.2f（<=0 或前置缺失）"),
			*GetNameSafe(TargetActor), TargetASC ? TEXT("有") : TEXT("无"),
			*GetNameSafe(*DamageGameplayEffectClass), Damage);
		return 0.0f;
	}

	// 权威门槛：伤害只在服务器结算。客户端预测激活时调这里会直接返回 0。
	if (TargetActor == nullptr || !TargetActor->HasAuthority())
	{
		UE_LOG(LogArcane, Warning, TEXT("[Damage] 被拦：Target=%s 非权威端"), *GetNameSafe(TargetActor));
		return 0.0f;
	}
	
	// 友军伤害门禁：同队（且双方都有合法阵营）直接免伤
	const int32 SourceTeam = GetActorTeamID(SourceActor);
	const int32 TargetTeam = GetActorTeamID(TargetActor);
	if (SourceTeam >= 0 && SourceTeam == TargetTeam)
	{
		UE_LOG(LogArcane, Log, TEXT("[Damage] 同队免伤：%s(队%d) → %s(队%d)"),
			*GetNameSafe(SourceActor), SourceTeam, *GetNameSafe(TargetActor), TargetTeam);
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
	
	UE_LOG(LogArcane, Log, TEXT("[Damage] 结算：%s → %s 伤害 %.2f"),
		*GetNameSafe(SourceActor), *GetNameSafe(TargetActor), Damage);
	
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
				if (IArcaneCombatant* Combatant = Cast<IArcaneCombatant>(TargetActor))
				{
					Combatant->PlayCombatHitReaction(ImpactRow.HitReactionTag);
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
	return ArcaneTeam::GetTeamId(Actor);
}

FVector UArcaneCombatFunctionLibrary::BuildDisplacementImpulse(const FArcaneImpactRow& ImpactRow,
	const FVector& SourceLocation, const FVector& TargetLocation, const FVector& HitNormal)
{
	FVector Direction = FVector::ZeroVector;
	switch (ImpactRow.DirectionMode)
	{
	case EArcaneDirectionMode::AlongHitNormal:
		Direction = -HitNormal;
		break;
	case EArcaneDirectionMode::AwayFromSource:
		// 只取平面方向：竖直交给 VerticalImpulse，否则击退会变成斜向上
		Direction = (TargetLocation - SourceLocation).GetSafeNormal2D();
		break;
	default:
		return FVector::ZeroVector;
	}
	
	return Direction * ImpactRow.HorizontalImpulse + FVector::UpVector * ImpactRow.VerticalImpulse;
}

AActor* UArcaneCombatFunctionLibrary::ResolveLockTarget(AActor* SourceActor, const FVector& FromLocation,
	float MaxRadius, AActor* ProposedTarget)
{
	if (SourceActor == nullptr || SourceActor->GetWorld() == nullptr)
	{
		return nullptr;
	}
	
	// 服务器只裁决，不替控制方做主：候选过不了校验就返回空，由调用方决定发直线弹。
	// 这里原本有一段"射程内最近的敌人"兜底，后果是玩家明明没指着人（客户端环内为空），
	// 弹却自己飞去找最近的敌人打 —— 施法范围失去意义，手感不可预期。
	if (IsLockableCharacter(SourceActor, FromLocation, ProposedTarget, MaxRadius))
	{
		return ProposedTarget;
	}
	
	return nullptr;
}

bool UArcaneCombatFunctionLibrary::IsLockableTarget(AActor* SourceActor, AActor* Candidate)
{
	// MaxRadius 传 -1 = 不比距离：客户端的"选敌范围"是 Preset 里的环形半径，
	// 服务器的"选敌范围"是 AbilityTable 的 Range，两边各管各的，规则本体共用这一个函数
	return IsLockableCharacter(SourceActor, SourceActor ? SourceActor->GetActorLocation() : FVector::ZeroVector, Candidate, -1.0f);
}

AActor* UArcaneCombatFunctionLibrary::GetFirstTargetingResult(const UWorld* World,
	FTargetingRequestHandle TargetingHandle)
{
	UTargetingSubsystem* Targeting = UTargetingSubsystem::Get(World);
	if (Targeting == nullptr)
	{
		return nullptr;
	}
	
	TArray<AActor*> Results;
	Targeting->GetTargetingResultsActors(TargetingHandle, Results);
	return Results.Num() > 0 ? Results[0] : nullptr;
}

FVector UArcaneCombatFunctionLibrary::GetTargetingAimPoint(FTargetingRequestHandle TargetingHandle)
{
	if (const FTargetingSourceContext* Context = FTargetingSourceContext::Find(TargetingHandle))
	{
		return Context->SourceLocation;
	}
	
	return FVector::ZeroVector;
}

bool UArcaneCombatFunctionLibrary::TryGetCursorGroundPoint(const APlayerController* PC, const AActor* IgnoreActor,
	float TraceDistance, float FallbackDistance, FVector& OutPoint)
{
	if (PC == nullptr)
	{
		return false;
	}
	
	FVector RayOrigin;
	FVector RayDirection;
	
	if (!PC->DeprojectMousePositionToWorld(RayOrigin, RayDirection))
	{
		return false;
	}
 
	UWorld* World = PC->GetWorld();
	if (World == nullptr)
	{
		return false;
	}
 
	// 与原来的 ArcaneLockCursor 完全一致：ECC_Pawn，忽略 PC 和施法者自己
	FHitResult Hit;
	FCollisionQueryParams Params(FName("ArcaneCursorGround"), false, PC);
	if (IgnoreActor != nullptr)
	{
		Params.AddIgnoredActor(IgnoreActor);
	}
 
	const FVector RayEnd = RayOrigin + RayDirection * TraceDistance;
	if (World->LineTraceSingleByChannel(Hit, RayOrigin, RayEnd, ECC_Pawn, Params))
	{
		OutPoint = Hit.ImpactPoint;
	}
	else
	{
		OutPoint = RayOrigin + RayDirection * FallbackDistance;
	}
 
	return true;
}

bool UArcaneCombatFunctionLibrary::IsLockableCharacter(AActor* SourceActor, const FVector& FromLocation,
                                                       AActor* Candidate, float MaxRadius)
{
	IArcaneCombatant* Combatant = Cast<IArcaneCombatant>(Candidate);
	if (Combatant == nullptr || Candidate == SourceActor || !Combatant->IsCombatAlive())
	{
		return false;
	}
	
	const int32 SourceTeam = GetActorTeamID(SourceActor);
	const int32 TargetTeam = GetActorTeamID(Candidate);
	// 无阵营（箱子/假人/未分队）与同队都不是锁定对象
	if (TargetTeam < 0 || (SourceTeam >= 0 && SourceTeam == TargetTeam))
	{
		return false;
	}
	
	// MaxRadius < 0 = 调用方不在这个函数里管距离
	if (MaxRadius >= 0.0f && FVector::Dist(FromLocation, Candidate->GetActorLocation()) > MaxRadius)
	{
		return false;
	}
	
	// 视线：从出膛点到目标胸部，命中目标自己不视为遮挡（把它加进 ignore，"打到它"与"通路"等价）
	FHitResult Blocker;
	FCollisionQueryParams Params(FName("ArcaneLockOn"), false, SourceActor);
	Params.AddIgnoredActor(Candidate);
	const FVector ToLocation = Candidate->GetActorLocation() + FVector(0.0f, 0.0f, ArcaneLockOnAimZ);
	
	return !SourceActor->GetWorld()->LineTraceSingleByChannel(Blocker, FromLocation, ToLocation, ECC_Visibility, Params);
}
