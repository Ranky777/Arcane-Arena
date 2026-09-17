// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "ArcaneCharacter.generated.h"

class UArcaneAbilitySystemComponent;

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
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	virtual void PossessedBy(AController* NewController) override;
	
	virtual void OnRep_PlayerState() override;
	
	// Bot 专用 ASC（玩家不用它，用 PlayerState 里的）
	UPROPERTY(VisibleAnywhere, Category = "Arcane|GAS")
	TObjectPtr<UArcaneAbilitySystemComponent> ArcaneASC;
	
	// 默认授予的能力（在 BP_CombatCharacter / BP_CombatEnemy 的 Class Defaults 里配）
	UPROPERTY(EditDefaultsOnly, Category = "Arcane|GAS")
	TArray<TSubclassOf<class UArcaneGameplayAbility>> DefaultAbilities;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
private:
	void GrantDefaultAbilities();
};
