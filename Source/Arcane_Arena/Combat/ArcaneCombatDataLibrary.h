// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArcaneCombatData.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ArcaneCombatDataLibrary.generated.h"

/**
 * 
 */
UCLASS()
class ARCANE_ARENA_API UArcaneCombatDataLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	// 两张表建议挂 GameInstanceSubsystem 或单独 UDataAsset 持有，避免到处硬引用
	UFUNCTION(BlueprintPure, Category = "Arcane|Data")
	static FArcaneAbilityRow GetAbilityRow(FName AbilityID, bool& bFound);
	
	UFUNCTION(BlueprintPure, Category = "Arcane|Data")
	static FArcaneImpactRow GetImpactRow(FName ImpactID, bool& bFound);
	
	UFUNCTION(BlueprintPure, Category = "Arcane|Data")
	static float SampleCurve(const UCurveTable* Table, FName RowName, float Input);
	
	UFUNCTION(BlueprintPure, Category = "Arcane|Data")
	static float SampleFalloffCurve(FName RowName, float Input);
};
