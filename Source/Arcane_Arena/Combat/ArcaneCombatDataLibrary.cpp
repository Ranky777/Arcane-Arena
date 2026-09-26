// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneCombatDataLibrary.h"

#include "ArcaneCombatData.h"

FArcaneAbilityRow UArcaneCombatDataLibrary::GetAbilityRow(FName AbilityID, bool& bFound)
{
	bFound = false;
	FArcaneAbilityRow Result;
	
	const UArcaneCombatData* Data = UArcaneCombatData::Get();
	
	if (Data == nullptr)
	{
		return Result;
	}
	
	const UDataTable* Table = Data->AbilityTable.LoadSynchronous();
	if (Table == nullptr)
	{
		return Result;
	}
	
	const FArcaneAbilityRow* Row = Table->FindRow<FArcaneAbilityRow>(AbilityID, TEXT("GetAbilityRow"));
	
	if (Row == nullptr)
	{
		return Result;
	}
	
	bFound = true;
	
	return *Row;
}

FArcaneImpactRow UArcaneCombatDataLibrary::GetImpactRow(FName ImpactID, bool& bFound)
{
	bFound = false;
	FArcaneImpactRow Result;
	
	const UArcaneCombatData* Data = UArcaneCombatData::Get();
	
	if (Data == nullptr)
	{
		return Result;
	}
	
	const UDataTable* Table = Data->ImpactTable.LoadSynchronous();
	if (Table == nullptr)
	{
		return Result;
	}
	
	const FArcaneImpactRow* Row = Table->FindRow<FArcaneImpactRow>(ImpactID, TEXT("GetImpactRow"));
	
	if (Row == nullptr)
	{
		return Result;
	}
	
	bFound = true;
	
	return *Row;
}

float UArcaneCombatDataLibrary::SampleCurve(const UCurveTable* Table, FName RowName, float Input)
{
	if (Table == nullptr)
	{
		return 1.0f;
	}
	
	const FRealCurve* Curve = Table->FindCurve(RowName, TEXT("SampleCurve"));
	
	if (Curve == nullptr)
	{
		return 1.0f;
	}
	
	return Curve->Eval(Input);
}

float UArcaneCombatDataLibrary::SampleFalloffCurve(FName RowName, float Input)
{
	const UArcaneCombatData* Data = UArcaneCombatData::Get();
	if (Data == nullptr)
	{
		return 1.0f;
	}
	
	const UCurveTable* Table = Data->FalloffCurves.LoadSynchronous();
	
	return SampleCurve(Table, RowName, Input);
}
