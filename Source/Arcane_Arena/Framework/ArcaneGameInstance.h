// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "ArcaneGameInstance.generated.h"

class UArcaneCombatData;
/**
 * 
 */
UCLASS()
class ARCANE_ARENA_API UArcaneGameInstance : public UGameInstance
{
	GENERATED_BODY()
	
public:
	virtual void Init() override;
	
protected:
	UPROPERTY(EditDefaultsOnly, Category = "Combat Data")
	TSoftObjectPtr<UArcaneCombatData> CombatDataAsset;
};
