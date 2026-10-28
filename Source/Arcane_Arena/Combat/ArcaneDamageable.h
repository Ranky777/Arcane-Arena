// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ArcaneDamageable.generated.h"

class UGameplayEffect;
/**
 * 一次近战命中的完整数据。
 * 由 UArcaneMeleeCombatComponent 在服务器/客户端各自算出；
 * 不复制、不存档，只用于一次接口调用。
 */
USTRUCT(BlueprintType)
struct FArcaneHitPayload
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadWrite)
	float Damage = 0.0f;
	
	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<AActor> DamageCauser = nullptr;
	
	UPROPERTY(BlueprintReadWrite)
	FVector HitLocation = FVector::ZeroVector;
	
	UPROPERTY(BlueprintReadWrite)
	FVector HitNormal = FVector::ZeroVector;
	
	UPROPERTY(BlueprintReadWrite)
	FVector Impulse = FVector::ZeroVector;
	
	UPROPERTY(BlueprintReadWrite)
	TSubclassOf<UGameplayEffect> DamageEffectClass;
};

/**
 * 所有“可被近战命中”的目标契约：
 * - Character：C++ 原生实现，走 GAS + LaunchCharacter
 * - 可破坏物：蓝图实现，沿用 CurrentHP / AddImpulseAtLocation / NS_Damage
 */
UINTERFACE(MinimalAPI, Blueprintable)
class UArcaneDamageable : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class ARCANE_ARENA_API IArcaneDamageable
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Arcane|Combat")
	void ApplyArcaneHit(const FArcaneHitPayload& Payload);
};
