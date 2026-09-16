// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneCharacter.h"

#include "ArcaneAbilitySystemComponent.h"


// Sets default values
AArcaneCharacter::AArcaneCharacter()
{
	bReplicates = true;
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

UAbilitySystemComponent* AArcaneCharacter::GetAbilitySystemComponent() const
{
	return GetArcaneASC();
}

// Called when the game starts or when spawned
void AArcaneCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AArcaneCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void AArcaneCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

