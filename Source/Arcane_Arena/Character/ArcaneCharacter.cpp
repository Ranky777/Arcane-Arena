// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneCharacter.h"

#include "AI/ArcaneAIController.h"
#include "Components/WidgetComponent.h"
#include "Core/ArcaneAbilityTypes.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/ArcaneAbilitySystemComponent.h"
#include "GAS/ArcaneGameplayAbility.h"
#include "GAS/ArcaneAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "Player/ArcanePlayerState.h"
#include "UI/ArcaneUserWidget.h"


// Sets default values
AArcaneCharacter::AArcaneCharacter()
{
	bReplicates = true;
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// 头顶血条，Screen 空间始终朝相机，俯视下最稳
	HealthBarComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBarComp"));
	HealthBarComp->SetupAttachment(GetRootComponent());
	HealthBarComp->SetWidgetSpace(EWidgetSpace::Screen);
	HealthBarComp->SetDrawSize(FVector2D(120.f, 14.f));   // [PLACEHOLDER] 手感后调
	HealthBarComp->SetRelativeLocation(FVector(0.0f, 0.0f, 110.0f));
	HealthBarComp->SetVisibility(true);
}

UAbilitySystemComponent* AArcaneCharacter::GetAbilitySystemComponent() const
{
	return GetArcaneASC();
}

UArcaneAbilitySystemComponent* AArcaneCharacter::GetArcaneASC() const
{
	// 玩家 → PlayerState 的 ASC；Bot 子类（AArcaneBotCharacter）覆写返回自己的
	if (const AArcanePlayerState* ArcanePS = GetPlayerState<AArcanePlayerState>())
	{
		return ArcanePS->GetArcaneASC();
	}

	return nullptr;
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
		// [DIAG] 服务端初始化失败：PlayerState ASC 为空
		// UE_LOG(LogTemp, Warning, TEXT("[DIAG-Arcane] InitAbilitySystem: GetArcaneASC() 返回 null（PlayerState/ASC 未就绪），跳过。Pawn=%s"), *GetName());
		return;
	}

	// [DIAG] 服务端初始化正常
	// UE_LOG(LogTemp, Warning, TEXT("[DIAG-Arcane] InitAbilitySystem OK: Pawn=%s ASC=%s"), *GetName(), *ASC->GetName());
	
	// 先解绑上一个 ASC 的 Health 委托（玩家 ASC 挂 PS，跨 Pawn 存活，防止重生重复绑定）
	if (BoundHealthASC)
	{
		BoundHealthASC->GetGameplayAttributeValueChangeDelegate(UArcaneAttributeSet::GetHealthAttribute()).RemoveAll(this);
	}
	
	if (const AArcaneAIController* AIController = Cast<AArcaneAIController>(GetController()))
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

	BoundHealthASC = ASC;
	// 绑定 Health 属性变化 → 死亡判定 / （未来）受击反馈
	ASC->GetGameplayAttributeValueChangeDelegate(UArcaneAttributeSet::GetHealthAttribute())
		.AddUObject(this, &AArcaneCharacter::OnHealthChanged);
	
	InitHealthBar();
}

void AArcaneCharacter::InitHealthBar()
{
	// 专用服务器没有 UI
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (HealthBarComp == nullptr)
	{
		return;
	}
	
	if (UArcaneUserWidget* WB = Cast<UArcaneUserWidget>(HealthBarComp->GetWidget()))
	{
		if (UAbilitySystemComponent* ASC = GetArcaneASC())
		{
			// [DIAG] 客户端血条绑定成功
			// UE_LOG(LogTemp, Warning, TEXT("[DIAG-Arcane] InitHealthBar 绑定成功: Pawn=%s Widget=%s ASC=%s"),
			// 	*GetName(), *WB->GetName(), *ASC->GetName());
			WB->InitFromASC(ASC);
			WB->BindAttribute(UArcaneAttributeSet::GetHealthAttribute());
			WB->BindAttribute(UArcaneAttributeSet::GetMaxHealthAttribute());
			GetWorld()->GetTimerManager().ClearTimer(HealthBarRetryHandle);
			
			return;
		}
		else
		{
			// [DIAG] Widget 存在但 GetArcaneASC() 为空（PlayerState/ASC 未就绪）
			// UE_LOG(LogTemp, Warning, TEXT("[DIAG-Arcane] InitHealthBar: Widget 存在但 GetArcaneASC()=null（Pawn=%s, 重试 %d/10）"),
			// 	*GetName(), HealthBarRetries);
		}
	}
	else
	{
		// // [DIAG] Widget 还没建好（GetWidget() 返回 null 或类型不对）
		// UE_LOG(LogTemp, Warning, TEXT("[DIAG-Arcane] InitHealthBar: GetWidget() 为 null 或不是 UArcaneUserWidget（Pawn=%s, 重试 %d/10）"),
		// 	*GetName(), HealthBarRetries);
	}
	
	// 客户端 PlayerState 可能还没复制到：0.1s 重试，最多 10 次
	if (++HealthBarRetries <= 10)
	{
		GetWorld()->GetTimerManager().SetTimer(HealthBarRetryHandle, FTimerDelegate::CreateUObject(this, &AArcaneCharacter::InitHealthBar), 0.1f, false);
	}
}



void AArcaneCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AArcaneCharacter, bIsDead);
}

void AArcaneCharacter::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	// // [DIAG] 服务端 Health 变化委托触发
	// UE_LOG(LogTemp, Warning, TEXT("[DIAG-Arcane] OnHealthChanged: Pawn=%s New=%.2f Old=%.2f HasAuth=%d bIsDead=%d"),
	// 	*GetName(), Data.NewValue, Data.OldValue, HasAuthority() ? 1 : 0, bIsDead ? 1 : 0);

	// 死亡判定只在权威端；客户端表现走 OnRep_IsDead
	if (HasAuthority() && Data.NewValue <= 0.0f && bIsDead == false)
	{
		HandleDeath();
	}
}

void AArcaneCharacter::OnRep_IsDead()
{
	if (bIsDead)
	{
		ApplyDeathPresentation();
	}
}

void AArcaneCharacter::HandleDeath()
{
	bIsDead = true;

	if (UAbilitySystemComponent* ASC = GetArcaneASC())
	{
		ASC->CancelAllAbilities();

		// 解耦点：回合系统订阅 Event.Character.Death，不轮询、不硬引用角色
		FGameplayEventData EventData;
		EventData.EventTag = FGameplayTag::RequestGameplayTag(FName("Event.Character.Death"));
		EventData.Target = this;
		ASC->HandleGameplayEvent(EventData.EventTag, &EventData);
	}

	ApplyDeathPresentation();
}

void AArcaneCharacter::ApplyDeathPresentation_Implementation()
{
	GetCharacterMovement()->DisableMovement();
	DisableInput(Cast<APlayerController>(GetController()));
	// [PLACEHOLDER] 阶段 6：接观战镜头与重生流程
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
		if (UArcaneAbilitySystemComponent* ASC = ArcanePS->GetArcaneASC())
		{
			ASC->InitAbilityActorInfo(ArcanePS, this);
		}
	}
	
	// 客户端血条绑定（此时 ASC 才可用）
	InitHealthBar();
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
			if (!AbilityClass)
			{
				continue;
			}

			// 排重，防止重生/重复 Possess 时同一能力被授予多份
			if (ASC->FindAbilitySpecFromClass(AbilityClass) != nullptr)
			{
				continue;
			}

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

// Called when the game starts or when spawned
void AArcaneCharacter::BeginPlay()
{
	Super::BeginPlay();

	// BP 子类在 Class Defaults 配 HealthBarWidgetClass；此处确保组件用上它
	if (HealthBarWidgetClass && HealthBarComp &&
		HealthBarComp->GetWidgetClass() != HealthBarWidgetClass)
	{
		HealthBarComp->SetWidgetClass(HealthBarWidgetClass);
		HealthBarComp->InitWidget();
	}

	// 客户端：WidgetComponent 控件已建好，立即尝试把血条绑到本角色 ASC。
	// 其他玩家（远程 Pawn）的 PlayerState/ASC 可能尚未复制到位 —— InitHealthBar 内含
	// 0.1s 重试（最多 10 次），无论 OnRep_PlayerState 先于/晚于此处触发都能绑上，
	// 避免“其他玩家头顶血条不绑定、攻击无变化、始终为空”的问题。
	// 服务器侧由 PossessedBy→InitAbilitySystem→InitHealthBar 负责，这里只补客户端半边。
	if (!HasAuthority())
	{
		InitHealthBar();
	}
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

