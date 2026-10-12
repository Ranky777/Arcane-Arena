// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ArcaneHUD.generated.h"

/**
 * 只负责创建全局 HUD 控件；阶段显隐等表现已全部收口到 UArcaneCombatHUDWidget（C++）。
 */
UCLASS()
class ARCANE_ARENA_API AArcaneHUD : public AHUD
{
	GENERATED_BODY()
	
public:
	AArcaneHUD();
	
	// 全局 HUD 控件，BP_ArcaneHUD 里指 WBP_ArcaneHUD
	UPROPERTY(EditDefaultsOnly, Category = "Arcane|UI")
	TSubclassOf<UUserWidget> GlobalHUDWidgetClass;
	
	UPROPERTY()
	TObjectPtr<UUserWidget> HUDWidget;
	
protected:
	virtual void BeginPlay() override;
};
