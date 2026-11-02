// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneDamageExecution.h"

#include "ArcaneAttributeSet.h"
#include "Arcane_Arena.h"

struct FArcaneDamageCaptures
{
	DECLARE_ATTRIBUTE_CAPTUREDEF(DamageReduction);
	DECLARE_ATTRIBUTE_CAPTUREDEF(Shield);
	
	FArcaneDamageCaptures()
	{
		DEFINE_ATTRIBUTE_CAPTUREDEF(UArcaneAttributeSet, DamageReduction, Target, false);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UArcaneAttributeSet, Shield, Target, false);
	}
};

static const FArcaneDamageCaptures& GetDamageCaptures()
{
	static FArcaneDamageCaptures Captures;
	return Captures;
}

UArcaneDamageExecution::UArcaneDamageExecution()
{
	RelevantAttributesToCapture.Add(GetDamageCaptures().DamageReductionDef);
	RelevantAttributesToCapture.Add(GetDamageCaptures().ShieldDef);
}

void UArcaneDamageExecution::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
                                                    FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	
	// ApplyArcaneDamage 注入的是正数基础伤害；符号只在这里处理
	const float BaseDamage = Spec.GetSetByCallerMagnitude(
		FGameplayTag::RequestGameplayTag(FName("Data.Damage")),
		false,
		0.0f
	);
	
	if (BaseDamage <= 0.0f)
	{
		return;
	}
	
	FAggregatorEvaluateParameters EvalParams;
	EvalParams.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	EvalParams.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();
	
	float Reduction = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(GetDamageCaptures().DamageReductionDef, EvalParams, Reduction);
	Reduction = FMath::Clamp(Reduction, 0.0f, 0.9f);
	
	float Shield = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(GetDamageCaptures().ShieldDef, EvalParams, Shield);
	Shield = FMath::Max(0.0f, Shield);
	
	const float AfterMitigation = BaseDamage * (1.0f - Reduction);
	const float Absorbed = FMath::Min(Shield, AfterMitigation);
	const float HealthDamage = AfterMitigation - Absorbed;
	
	UE_LOG(LogArcane, Log,
		TEXT("[DamageExec] Base=%.1f Reduction=%.0f%% Mitigated=%.1f Absorbed=%.1f HealthDamage=%.1f"),
		BaseDamage, Reduction * 100.0f, AfterMitigation, Absorbed, HealthDamage);
	
	if (Absorbed > 0.0f)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
			UArcaneAttributeSet::GetShieldAttribute(), EGameplayModOp::Additive, -Absorbed));
	}
	
	if (HealthDamage > 0.0f)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
			UArcaneAttributeSet::GetHealthAttribute(), EGameplayModOp::Additive, -HealthDamage));
	}
}
