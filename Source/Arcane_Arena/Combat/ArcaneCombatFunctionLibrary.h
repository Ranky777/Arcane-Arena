// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ArcaneCombatFunctionLibrary.generated.h"

class UGameplayEffect;
struct FHitResult;

/**
 * 战斗规则统一入口：伤害、（未来的）击退、治疗都从这里走。
 * GA 与环境 Hazard（熔岩/出界）均可调用。伤害仅在服务器生效，客户端调用返回 0。
 */
UCLASS()
class ARCANE_ARENA_API UArcaneCombatFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 对目标造成伤害并返回应用量（SetByCaller Data.Damage）。
	 * @param HitResult 命中信息（可为空结构体）；写入 Spec 供 GameplayCue 取命中位置。
	 */
	UFUNCTION(BlueprintCallable, Category = "Arcane|Combat")
	static float ApplyArcaneDamage(AActor* SourceActor, AActor* TargetActor,
		TSubclassOf<UGameplayEffect> DamageGameplayEffectClass, float Damage,
		const FHitResult& HitResult);
	
	// 取阵营 ID：玩家 Pawn→其 PlayerState 的 TeamID
	static int32 GetActorTeamID(const AActor* Actor);
};
