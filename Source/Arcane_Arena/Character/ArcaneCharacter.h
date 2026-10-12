// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "ArcaneCharacter.generated.h"

struct FGameplayTag;
class UArcaneUserWidget;
class UWidgetComponent;
class UArcaneAbilitySystemComponent;
struct FOnAttributeChangeData;

UCLASS()
class ARCANE_ARENA_API AArcaneCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AArcaneCharacter();
	
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	
	// 子类（PlayerState 归属 / Character 归属）各自覆盖，返回自己实际持有的 ASC
	virtual UArcaneAbilitySystemComponent* GetArcaneASC() const;

	// 服务器端：初始化 Owner/Avatar 并授予默认能力
	void InitAbilitySystem();

	// 绑定头顶血条到本角色 ASC（客户端 PlayerState 到位前自动重试）
	void InitHealthBar();
	
	// 受击反应蒙太奇入口，BP 子类实现（BP_CombatCharacter / BP_CombatEnemy）
	UFUNCTION(BlueprintImplementableEvent, Category = "Arcane|Combat")
	void PlayHitReaction(FGameplayTag ReactionTag);
	
	// 回合/房间阶段锁定：为 true 时不能移动、不能放技能（选阵营与结算期间）。
	// Replicated：服务端加锁后，客户端各自禁掉自己的输入（InputComponent 只在客户端有意义）。
	UPROPERTY(ReplicatedUsing = OnRep_MatchLocked, BlueprintReadOnly, Category = "Arcane|Match")
	bool bMatchLocked = false;
	
	UFUNCTION(BlueprintPure, Category = "Arcane|Match")
	bool IsMatchLocked() const { return bMatchLocked; }
	
	UFUNCTION(BlueprintCallable, Category = "Arcane|Match")
	void SetMatchLocked(bool bLocked);
	
	// 依据 GameState 当前阶段自动上锁。Pawn 生成时自愈，
	// 这样任何生成路径（首次出生 / 回合重生 / 客户端补生成）都覆盖到。
	void ApplyMatchLockFromGameState();
	
	// 锁定/死亡期间不响应任何自由移动输入。
	// Enhanced Input 的 IA_Move 直接调 AddMovementInput，不经过 AController::IgnoreMoveInput，
	// 所以只 SetIgnoreMoveInput(true) 是锁不住的，必须在输入入口拦。
	virtual void AddMovementInput(FVector WorldDirection, float ScaleValue = 1.0f, bool bForce = false) override;
	
protected:
	// IA_Jump 同理：锁定时不吃跳跃输入，避免起跳把 MovementMode 顶成 Falling。
	virtual void CheckJumpInput(float DeltaTime) override;
	
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	virtual void PossessedBy(AController* NewController) override;
	
	virtual void OnRep_PlayerState() override;
	
	UFUNCTION()
	void OnRep_MatchLocked();
	
	// 默认授予的能力（在 BP_CombatCharacter / BP_CombatEnemy 的 Class Defaults 里配）
	UPROPERTY(EditDefaultsOnly, Category = "Arcane|GAS")
	TArray<TSubclassOf<class UArcaneGameplayAbility>> DefaultAbilities;

	// 头顶血条（Screen 空间 WidgetComponent，跟随角色）
	UPROPERTY(BlueprintReadOnly, Category = "Arcane|UI")
	TObjectPtr<UWidgetComponent> HealthBarComp;
	
	UPROPERTY(EditDefaultsOnly, Category = "Arcane|UI")
	TSubclassOf<UArcaneUserWidget> HealthBarWidgetClass;
	
	// 死亡标记（权威端置位并复制；客户端经 OnRep 做表现）
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_IsDead, Category = "Arcane|State")
	bool bIsDead = false;
	
	void OnHealthChanged(const FOnAttributeChangeData& Data);
	
	UFUNCTION()
	void OnRep_IsDead();
	
	
	virtual void HandleDeath();
	
	// 死亡表现：禁移动/禁输入，服务器与客户端共用
	UFUNCTION(BlueprintNativeEvent, Category = "Arcane|Death")
	void ApplyDeathPresentation();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	// ASC 初始化后调用（Bot 在 PostInitializeComponents 拿到 Character ASC 处；
	// 玩家在 PlayerState 的 ASC 可用之后，两处都要绑）
	void BindStatusGates(UAbilitySystemComponent* ASC);
	
	UFUNCTION()
	void OnStunTagChanged(const FGameplayTag Tag, int32 NewCount);
	
public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

private:
	void GrantDefaultAbilities();
	
	void ApplyMatchLock();
	
	// 锁定期间每帧压制移动模式。Possess / 代理转自主 / 击退等引擎路径都会把
	// MovementMode 改回 Walking，一次性 DisableMovement 锁不住，必须持续补。
	void EnforceMatchLock();
	
	// 锁定或已死亡：不该自由移动/跳跃
	bool ShouldFreezeForMatch() const { return bMatchLocked || bIsDead; }
	
	// 当前绑定了 Health 委托的 ASC（防重生重复绑定）
	TObjectPtr<UArcaneAbilitySystemComponent> BoundHealthASC;
	
	// 血条绑定重试
	FTimerHandle HealthBarRetryHandle;
	int32 HealthBarRetries = 0;
};
