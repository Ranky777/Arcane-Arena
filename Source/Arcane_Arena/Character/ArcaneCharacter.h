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
	virtual UArcaneAbilitySystemComponent* GetArcaneASC() const { return nullptr; }

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
};
