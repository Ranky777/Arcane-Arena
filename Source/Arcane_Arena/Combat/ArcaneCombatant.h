// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ArcaneCombatant.generated.h"

struct FGameplayTag;
// This class does not need to be modified.
UINTERFACE()
class UArcaneCombatant : public UInterface
{
	GENERATED_BODY()
};

/**
 * 跨系统的“可战斗目标”契约。
 * 只暴露战斗层需要的语义，不暴露 AArcaneCharacter 的成员。
 */
class ARCANE_ARENA_API IArcaneCombatant
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual bool IsCombatAlive() const = 0;
	virtual bool IsCombatInputLocked() const = 0;
	
	// 调用方决定冲量方向；本函数只负责施加
	virtual void ApplyCombatKnockback(const FVector& Impulse) = 0;
	
	// 由 Combat 层在服务器调用，表现由角色实现
	virtual void PlayCombatHitReaction(const FGameplayTag& ReactionTag) = 0;
};
