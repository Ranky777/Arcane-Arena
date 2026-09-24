// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneAbilitySystemComponent.h"

#include "ArcaneGameplayAbility.h"


// Sets default values for this component's properties
UArcaneAbilitySystemComponent::UArcaneAbilitySystemComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
	AbilityCommittedCallbacks.AddUObject(this, &UArcaneAbilitySystemComponent::HandleAbilityCommitted);
}

bool UArcaneAbilitySystemComponent::GetCooldownTimes(EArcaneAbilityInputID InputID, float& Remaining, float& Duration) const
{
	Remaining = 0.0f;
	Duration = 0.0f;
	
	const FGameplayAbilitySpec* Spec = FindAbilitySpecFromInputID(static_cast<int32>(InputID));
	if (Spec == nullptr)
	{
		return false;
	}
	
	const UArcaneGameplayAbility* Ability = Cast<UArcaneGameplayAbility>(Spec->Ability);
	if (Ability == nullptr)
	{
		return false;
	}
	
	const FGameplayTagContainer* CooldownTags = Ability->GetCooldownTags();
	if (CooldownTags == nullptr || CooldownTags->IsEmpty())
	{
		return false;
	}
	
	// ⚠️ 冷却 Tag 经 DynamicGrantedTags 注入（授予到 ASC，供 CheckCooldown 阻塞），
	// 但 MakeQuery_MatchAnyEffectTags 只匹配"效果自身资产 Tag"，查不到运行时授予的 Tag。
	// 改用 OwningTagQuery：它同时匹配资产 Tag 与授予 Tag（含 DynamicGrantedTags），才能命中冷却 GE。
	FGameplayEffectQuery Query;
	Query.OwningTagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(*CooldownTags);
	
	const TArray<TPair<float, float>> TimesAndDurations = GetActiveEffectsTimeRemainingAndDuration(Query);
	
	// 可能命中多条 GE（共享冷却容器），取剩余时间最长的一条
	for (const TPair<float, float>& Pair : TimesAndDurations)
	{
		if (Pair.Key > Remaining)
		{
			Remaining = Pair.Key;
			Duration = Pair.Value;
		}
	}
	
	return Remaining > 0.0f;
}


// Called when the game starts
void UArcaneAbilitySystemComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}

void UArcaneAbilitySystemComponent::HandleAbilityCommitted(UGameplayAbility* Ability)
{
	if (const UArcaneGameplayAbility* ArcaneAbility = Cast<UArcaneGameplayAbility>(Ability))
	{
		OnAbilityCommitted.Broadcast();
	}
}
