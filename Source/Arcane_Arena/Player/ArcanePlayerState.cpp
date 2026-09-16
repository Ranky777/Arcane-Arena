// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcanePlayerState.h"

#include "GAS/ArcaneAbilitySystemComponent.h"
#include "GAS/ArcaneAttributeSet.h"

AArcanePlayerState::AArcanePlayerState()
{
	ArcaneASC = CreateDefaultSubobject<UArcaneAbilitySystemComponent>(TEXT("ArcaneASC"));
	ArcaneASC->SetIsReplicated(true);
	// 玩家用 Mixed：GE 片段预测 + 属性变化事件按需下发，性能/一致性平衡（架构文档 §5）
	ArcaneASC->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	
	AttributeSet = CreateDefaultSubobject<UArcaneAttributeSet>(TEXT("ArcaneAttributeSet"));
}

UAbilitySystemComponent* AArcanePlayerState::GetAbilitySystemComponent() const
{
	return ArcaneASC;
}
