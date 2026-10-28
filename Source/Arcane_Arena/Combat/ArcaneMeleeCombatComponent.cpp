// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneMeleeCombatComponent.h"

#include "ArcaneCombatData.h"
#include "ArcaneCombatDataLibrary.h"
#include "ArcaneCombatFunctionLibrary.h"
#include "ArcaneDamageable.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"


// Sets default values for this component's properties
UArcaneMeleeCombatComponent::UArcaneMeleeCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UArcaneMeleeCombatComponent::SetCurrentAttackAbilityID(FName AbilityID)
{
	CurrentAbilityID = AbilityID;
}

void UArcaneMeleeCombatComponent::PerformAttackTrace(FName SourceBone)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (Character == nullptr || Character->GetMesh() == nullptr || CurrentAbilityID.IsNone())
	{
		return;
	}
	
	bool bRowFound = false;
	const FArcaneAbilityRow Row = UArcaneCombatDataLibrary::GetAbilityRow(CurrentAbilityID, bRowFound);
	
	if (!bRowFound)
	{
		return;
	}
	
	// 表缺行时保持原蓝图行为：默认 ImpactRow = 无冲量/无状态，但伤害仍然结算。
	bool bImpactFound = false;
	const FArcaneImpactRow Impact = UArcaneCombatDataLibrary::GetImpactRow(Row.ImpactProfileID, bImpactFound);
	
	const FVector Start = Character->GetMesh()->GetSocketLocation(SourceBone);
	const FVector End = Start + Character->GetActorForwardVector() * Row.Range;
	
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(ObjectTypeQuery3);
	ObjectTypes.Add(ObjectTypeQuery2);
	
	TArray<FHitResult> Hits;
	const bool bHit = UKismetSystemLibrary::SphereTraceMultiForObjects(
		this,
		Start,
		End,
		Row.Radius,
		ObjectTypes,
		false,
		TArray<AActor*>(),
		EDrawDebugTrace::None,
		Hits,
		true,
		FLinearColor::Red,
		FLinearColor::Green,
		0.5f
	);
	
	if (!bHit)
	{
		return;
	}
	
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	const FVector LastUpdateLocation = Movement ? Movement->GetLastUpdateLocation() : Character->GetActorLocation();
	
	for (const FHitResult& Hit : Hits)
	{
		AActor* Target = Hit.GetActor();
		if (Target == nullptr || Target == Character)
		{
			continue;
		}
		
		const float FinalDamage = Row.BaseDamage * UArcaneCombatDataLibrary::SampleFalloffCurve(Row.DamageFalloffCurve, Hit.Distance);
		
		// 原蓝图：无论方向模式如何，都先加 Up * VerticalImpulse
		FVector Impulse = FVector::UpVector * Impact.VerticalImpulse;
		
		switch (Impact.DirectionMode)
		{
		case EArcaneDirectionMode::AwayFromSource:
			Impulse += (Hit.ImpactPoint - LastUpdateLocation).GetSafeNormal()
				* Impact.HorizontalImpulse;
			break;
		case EArcaneDirectionMode::AlongHitNormal:
			// 原蓝图：Normal * HorizontalImpulse * -1
			Impulse += Hit.Normal * (-Impact.HorizontalImpulse);
			break;
		case EArcaneDirectionMode::None:
		case EArcaneDirectionMode::Fixed:
		default:
			break;
		}
		
		FArcaneHitPayload Payload;
		Payload.Damage = FinalDamage;
		Payload.DamageCauser = Character;
		Payload.HitLocation = Hit.ImpactPoint;
		Payload.HitNormal = Hit.Normal;
		Payload.Impulse = Impulse;
		Payload.DamageEffectClass = Row.DamageEffectClass;
		
		if (Target->Implements<UArcaneDamageable>())
		{
			IArcaneDamageable::Execute_ApplyArcaneHit(Target, Payload);
		}
		
		UArcaneCombatFunctionLibrary::ApplyImpact(
			Character, Target, StatusEffectClass, Impact, Hit);
		
		if (HitCameraShakeClass != nullptr)
		{
			UGameplayStatics::PlayWorldCameraShake(
				this,
				HitCameraShakeClass,
				Hit.ImpactPoint,
				CameraShakeInnerRadius,
				CameraShakeOuterRadius
			);
		}
	}
}
