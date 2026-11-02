// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "ArcaneDamageExecution.generated.h"

/**
 * 伤害执行链的唯一算术处：
 *   基础伤害 → ×(1-减伤) → 护盾吸收 → 剩余扣 Health。
 * Combat 层只负责把基础伤害塞进 Spec，不再关心减伤/护盾/符号。
 */
UCLASS()
class ARCANE_ARENA_API UArcaneDamageExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()
	
public:
	UArcaneDamageExecution();
	
	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};
