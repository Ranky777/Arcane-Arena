#include "ArcaneBotCharacter.h"

#include "GAS/ArcaneAbilitySystemComponent.h"
#include "GAS/ArcaneAttributeSet.h"

AArcaneBotCharacter::AArcaneBotCharacter()
{
	ArcaneASC = CreateDefaultSubobject<UArcaneAbilitySystemComponent>(TEXT("ArcaneASC"));
	ArcaneASC->SetIsReplicated(true);
	ArcaneASC->SetReplicationMode(EGameplayEffectReplicationMode::Full); // Bot 全量同步

	// 属性集必须建在 ASC 的 Owner 上（即本 Actor），ASC 会在 InitAbilityActorInfo 时自动发现并注册；
	// 初值（Health=100 等）由 UArcaneAttributeSet 自身构造函数提供
	ArcaneAttributeSet = CreateDefaultSubobject<UArcaneAttributeSet>(TEXT("ArcaneAttributeSet"));
}

UArcaneAbilitySystemComponent* AArcaneBotCharacter::GetArcaneASC() const
{
	return ArcaneASC;
}

void AArcaneBotCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Bot 的 ASC 在客户端也要有 ActorInfo（服务器在 PossessedBy 已做，调用幂等）
	if (ArcaneASC)
	{
		ArcaneASC->InitAbilityActorInfo(this, this);
	}
}
