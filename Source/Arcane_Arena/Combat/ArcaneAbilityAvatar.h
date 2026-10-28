// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ArcaneAbilityAvatar.generated.h"

// This class does not need to be modified.
UINTERFACE()
class UArcaneAbilityAvatar : public UInterface
{
	GENERATED_BODY()
};

/**
 * GAS 能力 → 角色攻击入口的契约。
 * 能力类只认这个接口，不认 AArcaneCharacter。
 */
class ARCANE_ARENA_API IArcaneAbilityAvatar
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual void ExecutePrimaryAttack(FName AbilityID) = 0;
	virtual void ExecuteSecondaryAttack(FName AbilityID) = 0;
	virtual void ExecuteFinishSecondaryAttack() = 0;
	virtual void ExecuteHomingAttack(FName AbilityID, AActor* ProposedTarget, const FVector& AimPoint) = 0;
};
