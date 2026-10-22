// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArcaneProjectile.generated.h"

class UGameplayEffect;
class UProjectileMovementComponent;
class USphereComponent;
struct FArcaneAbilityRow;
/**
 * 服务器权威的跟踪弹（远程锁定技用）。
 *
 * 复制面只有位移：AbilityID / 目标 / 出膛点全部只存在于服务器，
 * 因为客户端既不判命中也不算伤害，复制过去没有读者。
 * 表现（受击特效）走伤害 GE 的 GameplayCue，由 GAS 自己复制，弹体不管。
 */
UCLASS(Abstract, Blueprintable)
class ARCANE_ARENA_API AArcaneProjectile : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AArcaneProjectile();
	
	/**
	 * 服务器发射专用：SpawnActorDeferred → InitShot → FinishSpawning。
	 * 必须在 FinishSpawning 之前调用：碰撞半径要早于第一次扫掠生效。
	 */
	void InitShot(AActor* InShooter, AActor* InTarget, FName InAbilityID, const FVector& InInitialDirection, float InMaxTravelDistance, const FArcaneAbilityRow& Row);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arcane|Combat")
	TObjectPtr<USphereComponent> CollisionComp;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arcane|Combat")
	TObjectPtr<UProjectileMovementComponent> MovementComp;
	
	// 命中后施加状态标签（眩晕/禁锢）用的通用时长 GE；BP_ArcaneProjectile 的 Class Defaults 拖 GE_Status
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arcane|Combat")
	TSubclassOf<UGameplayEffect> StatusEffectClass;
	
	virtual void BeginPlay() override;
	
	virtual void Tick(float DeltaTime) override;

private:
	// 以下全是服务器状态，一律不复制
	TWeakObjectPtr<AActor> Shooter;
	TWeakObjectPtr<AActor> HomingTarget;
	FName AbilityID;
	FVector FireOrigin = FVector::ZeroVector;
	
	float Speed = 0.0f;
	float TurnRateDeg = 0.0f; // 0 = 直线弹
	float MaxTravelDistance = 0.0f;
	float TraveledDistance = 0.0f;
	bool bFinished = false;
	
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
	
	void ApplyHitOnServer(AActor* Target, const FHitResult& Hit);
	
	// 限速转向：只改方向不改速率，弹道手感由 HomingTurnRate 一个数说话
	FVector SteerToward(const FVector& CurrentDir, const FVector& DesiredDir, float DeltaTime) const;
};
