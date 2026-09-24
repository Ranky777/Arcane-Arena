// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "ArcaneCharacter.generated.h"

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
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	virtual void PossessedBy(AController* NewController) override;
	
	virtual void OnRep_PlayerState() override;
	
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

	
public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

private:
	void GrantDefaultAbilities();
	
	// 当前绑定了 Health 委托的 ASC（防重生重复绑定）
	TObjectPtr<UArcaneAbilitySystemComponent> BoundHealthASC;
	
	// 血条绑定重试
	FTimerHandle HealthBarRetryHandle;
	int32 HealthBarRetries = 0;
};
