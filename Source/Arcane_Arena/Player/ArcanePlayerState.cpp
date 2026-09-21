// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcanePlayerState.h"

#include "GAS/ArcaneAbilitySystemComponent.h"
#include "GAS/ArcaneAttributeSet.h"
#include "Net/UnrealNetwork.h"

AArcanePlayerState::AArcanePlayerState()
{
	ArcaneASC = CreateDefaultSubobject<UArcaneAbilitySystemComponent>(TEXT("ArcaneASC"));
	ArcaneASC->SetIsReplicated(true);
	// 玩家保持 Mixed：属性只复制给拥有者本人，带宽友好（架构文档 §5）。
	// 注：Mixed 下其他玩家头顶血条看不到（非拥有者收不到对方 Health），若要让其他玩家血条
	// 也实时更新，需改为 Full 或单独 RepNotify 暴露 Health/MaxHealth；但本轮验证 owner 自身
	// 血条也空 + 自己 Health=0 不倒下，说明根因是更底层（属性变化委托/死亡判定），与复制模式无关。
	ArcaneASC->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	
	AttributeSet = CreateDefaultSubobject<UArcaneAttributeSet>(TEXT("ArcaneAttributeSet"));
}

UAbilitySystemComponent* AArcanePlayerState::GetAbilitySystemComponent() const
{
	return ArcaneASC;
}

void AArcanePlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AArcanePlayerState, TeamID);
}

void AArcanePlayerState::OnRep_Team()
{
	// [PLACEHOLDER] Phase 6：队伍染色 / 比分 UI 订阅点
}
