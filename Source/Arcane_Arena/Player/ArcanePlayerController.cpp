// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcanePlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Character/ArcaneCharacter.h"
#include "GAS/ArcaneAbilitySystemComponent.h"

AArcanePlayerController::AArcanePlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Crosshairs;
}

void AArcanePlayerController::BeginPlay()
{
	Super::BeginPlay();
	
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (DefaultMappingContext)
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void AArcanePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	BindAbilityInputs();
}

void AArcanePlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	
	// 服务器端 Possess 完成后输入映射已就绪；移动绑定留在 BP_CombatCharacter 的
	// 原有逻辑里不动（它已实现 IA_Move/IA_Jump，别重复绑定）
}

void AArcanePlayerController::BindAbilityInputs()
{
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
	{
		for (const FArcaneAbilityInputMapping& Mapping : AbilityInputMappings)
		{
			if (Mapping.InputAction != nullptr && Mapping.AbilityID != EArcaneAbilityInputID::None)
			{
				const int32 InputID = static_cast<int32>(Mapping.AbilityID);
				EIC->BindAction(Mapping.InputAction, ETriggerEvent::Started, this, &AArcanePlayerController::OnAbilityInputPressed, InputID);
			}
		}
	}
}

void AArcanePlayerController::OnAbilityInputPressed(int32 InputID)
{
	if (const AArcaneCharacter* Avatar = Cast<AArcaneCharacter>(GetPawn()))
	{
		if (UArcaneAbilitySystemComponent* ASC = Avatar->GetArcaneASC())
		{
			ASC->AbilityLocalInputPressed(InputID);
		}
	}
}
