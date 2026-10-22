// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Types/TargetingSystemTypes.h"
#include "ArcaneCombatFunctionLibrary.generated.h"

struct FArcaneImpactRow;
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
	
	UFUNCTION(BlueprintCallable, Category = "Arcane|Combat")
	static void ApplyImpact(AActor* SourceActor, AActor* TargetActor,
		TSubclassOf<UGameplayEffect> StatusEffectClass,
		const FArcaneImpactRow& ImpactRow, const FHitResult& HitResult);
	
	// 取阵营 ID：玩家 Pawn→其 PlayerState 的 TeamID
	static int32 GetActorTeamID(const AActor* Actor);
	
	/**
	 * 冲击配置 → 击退向量。近战/投射物共用同一套方向语义。
	 * AlongHitNormal 用法线；AwayFromSource 用"发射者→目标"的水平方向；
	 * 竖直分量一律走 VerticalImpulse。
	 */
	UFUNCTION(BlueprintCallable, Category = "Arcane|Combat")
	static FVector BuildDisplacementImpulse(const FArcaneImpactRow& ImpactRow,
		const FVector& SourceLocation, const FVector& TargetLocation, const FVector& HitNormal);
	
	/**
	 * 服务器唯一的"锁定"裁决口。ProposedTarget 只是客户端（或 Bot 焦点）的建议，
	 * 敌我/存活/距离/视线一律在服务器重算；
	 * 再没有则返回 nullptr —— 调用方据此决定发直线弹还是不发。
	 */
	UFUNCTION(BlueprintCallable, Category = "Arcane|Combat")
	static AActor* ResolveLockTarget(AActor* SourceActor, const FVector& FromLocation,
		float MaxRadius, AActor* ProposedTarget);
	
	/**
	 * 敌我/存活/视线规则的唯一实现处（距离不在这查）。
	 * 客户端选敌的过滤任务（蓝图）和服务器裁决（C++）调的是同一条规则，不写两遍。
	 */
	UFUNCTION(BlueprintCallable, Category = "Arcane|Combat")
	static bool IsLockableTarget(AActor* SourceActor, AActor* Candidate);
	
	/**
	 * 从选敌结果里取第一个 Actor（Preset 的 Sort 已把它排到最前）。
	 * 插件的 UTargetingSubsystem::Get 没标 UFUNCTION，蓝图拿不到子系统，
	 * 所以"Handle → Actor"这一步收敛在这里，全项目只写一遍。
	 */
	UFUNCTION(BlueprintCallable, Category = "Arcane|Combat")
	static AActor* GetFirstTargetingResult(const UWorld* World, FTargetingRequestHandle TargetingHandle);
	
	/**
	 * 读回本次选敌的圆心（由 UArcaneTargetingSelectionTask_Lock 写入数据仓库）。
	 * 有目标时它是瞄准窗口中心，没目标时它就是"玩家/ Bot 想打的那个点"，
	 * 所以空技能也要拿它当方向，而不是用角色朝向碰运气。
	 */
	UFUNCTION(BlueprintCallable, Category = "Arcane|Combat")
	static FVector GetTargetingAimPoint(FTargetingRequestHandle TargetingHandle);
	
	/**
	 * 本地鼠标光标 → 地面点。
	 * 锁定选敌的圆心和范围预览共用这一条射线，保证"画出来的圈"与"实际选的圆心"是同一个点。
	 * @return false = 无法反投影（无 PC / 光标不可用），OutPoint 不保证有效。
	 */
	static bool TryGetCursorGroundPoint(const APlayerController* PC, const AActor* IgnoreActor, 
		float TraceDistance, float FallbackDistance, FVector& OutPoint);
	
private:
	// 单个候选是否可被锁定：是 ArcaneCharacter + 存活 + 敌对 + 在半径内 + 有视线。
	// MaxRadius < 0 = 不比距离（客户端环形半径由 Preset 管，服务器射程由这里管）
	static bool IsLockableCharacter(AActor* SourceActor, const FVector& FromLocation,
		AActor* Candidate, float MaxRadius);
};
