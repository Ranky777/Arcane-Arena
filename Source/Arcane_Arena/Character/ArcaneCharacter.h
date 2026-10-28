// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Combat/ArcaneAbilityAvatar.h"
#include "Combat/ArcaneCombatant.h"
#include "Combat/ArcaneDamageable.h"
#include "GameFramework/Character.h"
#include "ArcaneCharacter.generated.h"

class UArcaneCameraLeadComponent;
class UArcaneMeleeCombatComponent;
struct FGameplayTag;
class UArcaneUserWidget;
class UWidgetComponent;
class UArcaneAbilitySystemComponent;
struct FOnAttributeChangeData;

UCLASS()
class ARCANE_ARENA_API AArcaneCharacter : public ACharacter, public IAbilitySystemInterface, public IArcaneCombatant, public IArcaneAbilityAvatar, public IArcaneDamageable
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AArcaneCharacter();
	
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	
	// 子类（PlayerState 归属 / Character 归属）各自覆盖，返回自己实际持有的 ASC
	virtual UArcaneAbilitySystemComponent* GetArcaneASC() const;
	
	// IArcaneCombatant
	virtual bool IsCombatAlive() const override;
	virtual bool IsCombatInputLocked() const override;
	virtual void ApplyCombatKnockback(const FVector& Impulse) override;
	virtual void PlayCombatHitReaction(const FGameplayTag& ReactionTag) override;
	
	// IArcaneAbilityAvatar
	virtual void ExecutePrimaryAttack(FName AbilityID) override;
	virtual void ExecuteSecondaryAttack(FName AbilityID) override;
	virtual void ExecuteFinishSecondaryAttack() override;
	virtual void ExecuteHomingAttack(FName AbilityID, AActor* ProposedTarget, const FVector& AimPoint) override;
	
	// IArcaneDamageable：近战伤害的 C++ 目标实现
	virtual void ApplyArcaneHit_Implementation(const FArcaneHitPayload& Payload) override;
	
	UFUNCTION(BlueprintPure, Category = "Arcane|Combat")
	UArcaneMeleeCombatComponent* GetMeleeCombatComponent() const;

	// 服务器端：初始化 Owner/Avatar 并授予默认能力
	void InitAbilitySystem();

	// 绑定头顶血条到本角色 ASC（客户端 PlayerState 到位前自动重试）
	void InitHealthBar();
	
	// 受击反应蒙太奇入口，BP 子类实现（BP_CombatCharacter / BP_CombatEnemy）
	UFUNCTION(BlueprintImplementableEvent, Category = "Arcane|Combat")
	void PlayHitReaction(FGameplayTag ReactionTag);
	
	// —— 技能与角色之间的攻击契约：GA 只认这个，不认任何具体 BP 类。
	// 玩家（BP_CombatCharacter）与 Bot（BP_CombatEnemy）各实现一次，人机同规则。
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Arcane|Combat")
	void BeginPrimaryAttack(FName AbilityID);
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Arcane|Combat")
	void BeginSecondaryAttack(FName AbilityID);
	
	// 蓄力型技能的"松手"信号；不需要它的子类留空实现即可
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Arcane|Combat")
	void FinishSecondaryAttack();
	
	// 远程锁定技契约。ProposedTarget 是"建议"而非结论：
	// 玩家=客户端本地选敌管线算出的，Bot=服务器跑同一条管线的结果，两者都要经服务器重算。
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Arcane|Combat")
	void BeginHomingAttack(FName AbilityID, AActor* ProposedTarget, FVector AimPoint);
	
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
	
	// 死亡标记（权威端置位并复制；客户端经 OnRep 做表现）
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_IsDead, Category = "Arcane|State")
	bool bIsDead = false;
	
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
	
	// 近战结算组件：trace / 伤害 / 冲击 / CameraShake 都在里面
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arcane|Combat")
	TObjectPtr<UArcaneMeleeCombatComponent> MeleeCombatComponent;
	
	// 本地玩家相机 lead
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arcane|Camera")
	TObjectPtr<UArcaneCameraLeadComponent> CameraLeadComponent;
	
	UPROPERTY(EditDefaultsOnly, Category = "Arcane|UI")
	TSubclassOf<UArcaneUserWidget> HealthBarWidgetClass;
	
	// —— 远程锁定技：发射与 RPC ——
	// 服务器唯一发射口：主机本地激活、客户端 RPC、Bot 服务器激活三条路都收敛到这里
	void LaunchHomingAttack(FName AbilityID, AActor* ProposedTarget, const FVector& AimPoint);
	
	// 本次远程射击的瞄准偏移（世界空间，cm）。基类返回零 = 完美瞄准；
	// Bot 子类向 AI Controller 取一个带随机误差的偏移。玩家与 Bot 共用同一条发射链路。
	virtual FVector GetHomingAimOffset() const;	
	
	// Server RPC 由引擎保证"只能由本 Actor 的 owning connection 发起"，
	// 但"报哪个 Actor 当目标"不可信，所以真伪一律交给 LaunchHomingAttack 内的 ResolveLockTarget
	UFUNCTION(Server, Reliable)
	void ServerRequestHomingAttack(FName AbilityID, AActor* ProposedTarget, FVector AimPoint);
	
	// 出膛点 = ActorLocation + Forward*SpawnForward + Up*SpawnHeight。
	// 不依赖骨骼插槽：玩家骨架与 Bot 骨架不同也能共用一条规则
	UPROPERTY(EditDefaultsOnly, Category = "Arcane|Combat", meta = (ClampMin = "0.0"))
	float ProjectileSpawnForward = 80.0f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Arcane|Combat", meta = (ClampMin = "0.0"))
	float ProjectileSpawnHeight = 60.0f;
	
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
