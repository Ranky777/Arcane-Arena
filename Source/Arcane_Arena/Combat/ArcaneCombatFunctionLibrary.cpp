// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneCombatFunctionLibrary.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"

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
