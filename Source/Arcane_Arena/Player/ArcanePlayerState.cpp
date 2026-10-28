// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcanePlayerState.h"

#include "Core/ArcaneTeamUtils.h"
#include "Framework/ArcaneGameState.h"
#include "GAS/ArcaneAbilitySystemComponent.h"
#include "GAS/ArcaneAttributeSet.h"
#include "Net/UnrealNetwork.h"

AArcanePlayerState::AArcanePlayerState()
{
	ArcaneASC = CreateDefaultSubobject<UArcaneAbilitySystemComponent>(TEXT("ArcaneASC"));
	ArcaneASC->SetIsReplicated(true);
	// Mixed 只影响 ActiveGameplayEffects 的复制，不影响 AttributeSet 属性的复制。
	// AttributeSet 由 UAbilitySystemComponent::ReplicateSubobjects 以 COND_None
	// 复制给所有客户端，因此其他玩家头顶血条能正常读到 Health。
	ArcaneASC->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	
	AttributeSet = CreateDefaultSubobject<UArcaneAttributeSet>(TEXT("ArcaneAttributeSet"));
}

UAbilitySystemComponent* AArcanePlayerState::GetAbilitySystemComponent() const
{
	return ArcaneASC;
}

FGenericTeamId AArcanePlayerState::GetGenericTeamId() const
{
	return TeamID >= 0 && TeamID < 255 ? FGenericTeamId(static_cast<uint8>(TeamID)) : FGenericTeamId::NoTeam;
}

ETeamAttitude::Type AArcanePlayerState::GetTeamAttitudeTowards(const AActor& Other) const
{
	return ArcaneTeam::GetAttitude(TeamID, ArcaneTeam::GetTeamId(&Other));
}

void AArcanePlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AArcanePlayerState, TeamID);
	DOREPLIFETIME(AArcanePlayerState, bIsDead);
	DOREPLIFETIME(AArcanePlayerState, bIsRoomOwner);
	DOREPLIFETIME(AArcanePlayerState, bIsBot);
}

void AArcanePlayerState::OnRep_Team()
{
	// 客户端：队伍变化要驱动大厅面板重新统计人数。
	// OnRosterChanged 是服务器侧 Broadcast 的动态委托，不会自己过网，
	// 这里在 OnRep 落地后本地补广播一次。
	if (AArcaneGameState* GS = GetWorld() ? GetWorld()->GetGameState<AArcaneGameState>() : nullptr)
	{
		GS->OnRosterChanged.Broadcast();
	}
}
