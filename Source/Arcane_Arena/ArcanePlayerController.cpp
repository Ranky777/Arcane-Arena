// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcanePlayerController.h"

AArcanePlayerController::AArcanePlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Crosshairs;
}

void AArcanePlayerController::BeginPlay()
{
	Super::BeginPlay();
}
