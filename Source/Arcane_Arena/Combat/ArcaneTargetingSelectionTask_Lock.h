// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Tasks/TargetingSelectionTask_AOE.h"
#include "ArcaneTargetingSelectionTask_Lock.generated.h"

/**
 * 锁定技的"圆心"唯一来源：算出来，并写回 FTargetingSourceContext.SourceLocation。
 * 排序任务读同一个值，于是"画的圈 / 捞人的圈 / 谁离得更近"用的是同一个点。
 *
 * 圆心按控制方分三种，语义同构 —— 环永远只是"我要打这一点附近"的容差窗口：
 *   本地玩家 → 光标射线命中点（什么都没打到就用射线 * FallbackDistance 兜一个有限远的点）
 *   Bot     → AI 的敌对目标位置（ResolveEnemyTarget，自带阶段与视野门控）
 *   其它     → 自身位置（观战、无控制器、无目标）
 *
 * 注意：这里的 Radius 是瞄准窗口，不是施法范围。施法范围 = AbilityTable 的 Range
 * （它同时是弹体最大飞行距离，以及服务器校验目标是否越界的那道门槛）。
 */
UCLASS()
class ARCANE_ARENA_API UArcaneTargetingSelectionTask_Lock : public UTargetingSelectionTask_AOE
{
	GENERATED_BODY()
	
public:
	UArcaneTargetingSelectionTask_Lock(const FObjectInitializer& ObjectInitializer);

	// 预览/调试用：本任务实际生效的选敌窗口半径（默认 250，Preset 可覆盖）。
	// 与 AOE 任务内部 MakeSphere 用的是同一个值（Radius.GetValue()），不会出现两份配置。
	float GetLockRadius() const { return Radius.GetValue(); }
	
protected:
	// 光标射线打多远
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arcane|Targeting", meta = (ClampMin = "0.0"))
	float CursorTraceDistance = 4000.0f;
	
	// 射线什么都没打到时，沿射线取这么远的一个点当圆心（别让环心跑到无穷远）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arcane|Targeting", meta = (ClampMin = "0.0"))
	float CursorFallbackDistance = 2000.0f;
	
	// 覆写父类的 BlueprintNativeEvent：BP 子类不要再实现这个事件，
	// 否则整段 C++ 被盖掉，而且没人把圆心写回数据仓库，排序会静默退化
	virtual FVector GetSourceLocation_Implementation(const FTargetingRequestHandle& TargetingHandle) const override;
	
private:
	FVector ResolveLockCenter(const FTargetingRequestHandle& TargetingHandle) const;
};
