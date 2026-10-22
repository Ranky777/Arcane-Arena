// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneAbilitySystemComponent.h"

#include "Arcane_Arena.h"
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

void UArcaneAbilitySystemComponent::ClearAllCooldowns()
{
	// 冷却 Tag 不是 GE_Cooldown 的静态资产 Tag，而是 ApplyCooldown 注入 Spec 的 DynamicGrantedTags。
	// 因此必须用 OwningTags 查询（RemoveActiveEffectsWithGrantedTags 内部正是如此）；
	// MakeQuery_MatchAnyEffectTags 只看效果资产 Tag，会一个都删不掉。
	FGameplayTagContainer AllCooldownTags;
	
	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		const UGameplayAbility* Ability = Spec.GetPrimaryInstance();
		if (Ability == nullptr)
		{
			Ability = Spec.Ability.Get();
		}
		
		if (const UArcaneGameplayAbility* ArcaneAbility = Cast<UArcaneGameplayAbility>(Ability))
		{
			if (const FGameplayTagContainer* Tags = ArcaneAbility->GetCooldownTags())
			{
				AllCooldownTags.AppendTags(*Tags);
			}
		}
	}
	
	if (!AllCooldownTags.IsEmpty())
	{
		const int32 Removed = RemoveActiveEffectsWithGrantedTags(AllCooldownTags);
		UE_LOG(LogArcane, Verbose, TEXT("[GAS] ClearAllCooldowns: 清理 %d 个冷却效果（%s）"),
			Removed, *AllCooldownTags.ToStringSimple());
	}
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
		// 提交 = GAS 已走完 CanActivate/Cost/Cooldown 并接受这次激活。
		// 「按了输入却没这条日志」= 门禁在 GAS 内部；「有这条但没表现」= 门禁在技能蓝图里。
		UE_LOG(LogArcane, Log, TEXT("[GAS] %s 提交能力 %s（输入槽 %d）"),
			*GetNameSafe(GetOwner()), *GetNameSafe(Ability), static_cast<int32>(ArcaneAbility->InputID));
		
		OnAbilityCommitted.Broadcast();
	}
}
