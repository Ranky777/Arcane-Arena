// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ArcanePlayerController.generated.h"

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
};
