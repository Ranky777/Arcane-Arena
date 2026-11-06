// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneMeleeCombatComponent.h"

#include "ArcaneCombatData.h"
#include "ArcaneCombatDataLibrary.h"
#include "ArcaneCombatFunctionLibrary.h"
#include "ArcaneDamageable.h"
#include "Character/ArcaneCharacter.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Net/ArcaneLagCompensationSubsystem.h"


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
	
	// 客户端不再本地跑同一条伤害管线：
	// 命中与位移由服务器判定，客户端只接收服务器 Multicast 的表现。
	// 这样高延迟下不会出现“本地先击退、服务器再纠偏”的双重结果。
	if (!Character->HasAuthority())
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
	
	// 1) 先跑一次当前时刻 trace：箱子/假人/可破坏物等非角色目标仍然走原逻辑。
	TArray<FHitResult> CurrentHits;
	UKismetSystemLibrary::SphereTraceMultiForObjects(
		this,
		Start,
		End,
		Row.Radius,
		ObjectTypes,
		false,
		TArray<AActor*>(),
		EDrawDebugTrace::None,
		CurrentHits,
		true,
		FLinearColor::Red,
		FLinearColor::Green,
		0.5f
	);
	
	AArcaneCharacter* ArcaneCharacter = Cast<AArcaneCharacter>(Character);
	UArcaneLagCompensationSubsystem* LagComp = ArcaneCharacter
		? Character->GetWorld()->GetSubsystem<UArcaneLagCompensationSubsystem>()
		: nullptr;
 
	const float RewindSeconds = LagComp
		? LagComp->ComputeRewindSeconds(ArcaneCharacter)
		: 0.0f;
	const bool bUseLagComp = LagComp != nullptr && RewindSeconds > 0.0f;
 
	TArray<FHitResult> Hits;
	
	for (const FHitResult& Hit : CurrentHits)
	{
		AActor* Target = Hit.GetActor();
		if (Target == nullptr || Target == Character)
		{
			continue;
		}
		
		// 角色目标如果有历史，交给回退扫掠重新判定；没有历史的角色保留当前结果。
		if (bUseLagComp)
		{
			const AArcaneCharacter* TargetCharacter = Cast<AArcaneCharacter>(Target);
			if (TargetCharacter != nullptr && LagComp->HasHistory(TargetCharacter))
			{
				continue;
			}
		}
 
		Hits.Add(Hit);
	}
	
	// 2) 高延迟客户端：把角色胶囊回退到“客户端当时看到的时间”，
	//    再做一次近战扫掠。当前 trace 没打中的目标，历史位置可能打中；反过来也一样。
	if (bUseLagComp)
	{
		TArray<FHitResult> HistoricalHits;
		LagComp->SweepMeleeAgainstHistory(
			ArcaneCharacter, Start, End, Row.Radius, RewindSeconds, HistoricalHits);
		Hits.Append(HistoricalHits);
	}
 
	if (Hits.Num() == 0)
	{
		return;
	}
 
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	const FVector LastUpdateLocation = Movement
		? Movement->GetLastUpdateLocation()
		: Character->GetActorLocation();
 
	for (const FHitResult& Hit : Hits)
	{
		AActor* Target = Hit.GetActor();
		if (Target == nullptr || Target == Character)
		{
			continue;
		}
 
		const float FinalDamage = Row.BaseDamage
			* UArcaneCombatDataLibrary::SampleFalloffCurve(Row.DamageFalloffCurve, Hit.Distance);
 
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
	}
}
