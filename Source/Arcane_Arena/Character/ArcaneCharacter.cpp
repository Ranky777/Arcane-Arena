// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneCharacter.h"

#include "AI/ArcaneAIController.h"
#include "Core/ArcaneAbilityTypes.h"
#include "GAS/ArcaneAbilitySystemComponent.h"
#include "GAS/ArcaneGameplayAbility.h"
#include "Player/ArcanePlayerState.h"


// Sets default values
AArcaneCharacter::AArcaneCharacter()
{
	bReplicates = true;
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	ArcaneASC = CreateDefaultSubobject<UArcaneAbilitySystemComponent>(TEXT("ArcaneASC"));
	ArcaneASC->SetIsReplicated(true);
	
	// Bot 用 Full：AI 数量少且无预测需求，全量同步最省心
	ArcaneASC->SetReplicationMode(EGameplayEffectReplicationMode::Full);
}

UAbilitySystemComponent* AArcaneCharacter::GetAbilitySystemComponent() const
{
	return GetArcaneASC();
}

UArcaneAbilitySystemComponent* AArcaneCharacter::GetArcaneASC() const
{
	// 有有效 PlayerState 的算玩家 → 用 PlayerState 的 ASC；否则（Bot）用自己的
	if (const AArcanePlayerState* ArcanePS = GetPlayerState<AArcanePlayerState>())
	{
		return ArcanePS->GetArcaneASC();
	}
	
	return ArcaneASC;
}

void AArcaneCharacter::InitAbilitySystem()
{
	if (HasAuthority() == false)
	{
		return;
	}
	
	UArcaneAbilitySystemComponent* ASC = GetArcaneASC();
	if (ASC == nullptr)
	{
		return;
	}
	
	if (AArcaneAIController* AIController = Cast<AArcaneAIController>(GetController()))
	{
		// Bot 路径：Owner = Avatar = 自己
		ASC->InitAbilityActorInfo(this, this);
	}
	else
	{
		// 玩家路径：Owner = PlayerState，Avatar = Character
		if (AArcanePlayerState* ArcanePS = GetPlayerState<AArcanePlayerState>())
		{
			ASC->InitAbilityActorInfo(ArcanePS, this);
		}
	}
	
	GrantDefaultAbilities();
}

void AArcaneCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	
	// PossessedBy 只在服务器跑，正好做权威端初始化
	InitAbilitySystem();
}

void AArcaneCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	
	// 客户端：PlayerState 复制到位后补一次 ActorInfo（玩家路径的客户端半边）
	if (AArcanePlayerState* ArcanePS = GetPlayerState<AArcanePlayerState>())
	{
		ArcanePS->GetArcaneASC()->InitAbilityActorInfo(ArcanePS, this);
	}
}

void AArcaneCharacter::GrantDefaultAbilities()
{
	if (DefaultAbilities.Num() == 0)
	{
		return;
	}
	
	if (UArcaneAbilitySystemComponent* ASC = GetArcaneASC())
	{
		for (const TSubclassOf<UArcaneGameplayAbility>& AbilityClass : DefaultAbilities)
		{
			if (AbilityClass)
			{
				FGameplayAbilitySpec Spec { TSubclassOf<UGameplayAbility>(AbilityClass) };
				Spec.Level = 1;

				// 关键：AbilityLocalInputPressed 按 Spec.InputID 匹配（不是 GA 的 InputID 属性），
				// 必须把能力 CDO 上配置的 InputID 填进 Spec，否则输入永远匹配不到
				if (const UArcaneGameplayAbility* AbilityCDO = AbilityClass.GetDefaultObject())
				{
					Spec.InputID = static_cast<int32>(AbilityCDO->InputID);
				}
				else
				{
					Spec.InputID = static_cast<int32>(EArcaneAbilityInputID::None);
				}

				Spec.SourceObject = GetOwner();
				ASC->GiveAbility(Spec);
			}
		}
	}
}

// Called when the game starts or when spawned
void AArcaneCharacter::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void AArcaneCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void AArcaneCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

