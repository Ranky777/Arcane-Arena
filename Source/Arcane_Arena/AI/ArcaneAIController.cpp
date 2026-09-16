// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneAIController.h"


// Sets default values
AArcaneAIController::AArcaneAIController()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AArcaneAIController::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AArcaneAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

