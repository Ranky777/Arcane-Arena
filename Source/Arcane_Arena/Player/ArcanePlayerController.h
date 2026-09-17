// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GAS/ArcaneGameplayAbility.h"
#include "ArcanePlayerController.generated.h"

class UInputMappingContext;
class UInputAction;

USTRUCT(BlueprintType)
struct FArcaneAbilityInputMapping
{
	GENERATED_BODY()
	
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<UInputAction> InputAction = nullptr;
	
	UPROPERTY(EditDefaultsOnly)
	EArcaneAbilityInputID AbilityID = EArcaneAbilityInputID::None;
};

/**
 * 
 */
UCLASS()
class ARCANE_ARENA_API AArcanePlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	AArcanePlayerController();
	
protected:
	virtual void BeginPlay() override; 
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;
	
	UPROPERTY(EditDefaultsOnly, Category = "Arcane|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext = nullptr; // 配 IMC_Default + IMC_Combat 合并版
	
	// 能力输入映射（在 BP_ArcanePlayerController 里逐条配 IA→InputID）
	UPROPERTY(EditDefaultsOnly, Category = "Arcane|Input")
	TArray<FArcaneAbilityInputMapping> AbilityInputMappings;
	
private:
	void BindAbilityInputs();

	// 必须是 UFUNCTION：BindAction 带参重载在运行时按 UFunction 名字解析
	UFUNCTION()
	void OnAbilityInputPressed(int32 InputID);
};
