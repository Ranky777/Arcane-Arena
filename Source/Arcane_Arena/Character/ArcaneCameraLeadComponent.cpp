// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneCameraLeadComponent.h"

#include "GameFramework/Character.h"
#include "GameFramework/SpringArmComponent.h"


// Sets default values for this component's properties
UArcaneCameraLeadComponent::UArcaneCameraLeadComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UArcaneCameraLeadComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerCharacter = Cast<ACharacter>(GetOwner());
	
	if (OwnerCharacter.IsValid())
	{
		CameraBoom = OwnerCharacter->FindComponentByClass<USpringArmComponent>();
	}
}


// Called every frame
void UArcaneCameraLeadComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                               FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ACharacter* Character = OwnerCharacter.Get();
	USpringArmComponent* Boom = CameraBoom.Get();
	
	// Bot 没有 PlayerController，远端代理也不是本地控制，直接跳过
	if (Character == nullptr || Boom == nullptr || !Character->IsLocallyControlled())
	{
		return;
	}
	
	APlayerController* PC = Cast<APlayerController>(Character->GetController());
	if (PC == nullptr || !PC->IsLocalController())
	{
		return;
	}
	
	FHitResult Hit;
	if (!PC->GetHitResultUnderCursorByChannel(TraceTypeQuery1, true, Hit))
	{
		return;
	}
	
	const FVector ActorLocation = Character->GetActorLocation();
	const FVector LookAtLocation = ActorLocation + FVector(0.0f, 0.0f, LookAtHeight);
	
	// 视角 Yaw 跟随光标；Pitch 固定 0，由 SpringArm 自己锁 -65
	const FRotator LookAtRotation = (Hit.ImpactPoint - LookAtLocation).Rotation();
	const FRotator TargetRotation(0.0f, LookAtRotation.Yaw, 0.0f);
	
	PC->SetControlRotation(FMath::RInterpConstantTo(
		Character->GetActorRotation(),
		TargetRotation,
		DeltaTime,
		RotationInterpSpeed
	));
	
	// 相机 lead：按当前臂长在 ZoomMin~ZoomMax 的比例，取 CameraLeadMax 的一段
	const float ZoomRange = ZoomMax - ZoomMin;
	const float ZoomAlpha = ZoomRange > KINDA_SMALL_NUMBER
		? (Boom->TargetArmLength - ZoomMin) / ZoomRange
		: 0.0f;
 
	FVector LeadDirection(
		Hit.ImpactPoint.X - ActorLocation.X,
		Hit.ImpactPoint.Y - ActorLocation.Y,
		0.0f);
	LeadDirection = LeadDirection.GetSafeNormal();
 
	const FVector TargetLocation =
		ActorLocation + LeadDirection * (ZoomAlpha * CameraLeadMax);
 
	Boom->SetWorldLocation(FMath::VInterpTo(
		Boom->GetComponentLocation(),
		TargetLocation,
		DeltaTime,
		LeadInterpSpeed));
 
	Boom->SetWorldRotation(FRotator(FixedPitch, 0.0f, 0.0f));
}

