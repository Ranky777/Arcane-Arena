// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Tasks/TargetingSortTask_Base.h"
#include "ArcaneTargetingSortTask_LockDistance.generated.h"

/**
 * 按"离圆心的距离"升序排序：环里同时有多个合法目标时，选离瞄准点最近的那个。
 *
 * 为什么继承 UTargetingSortTask_Base 而不是现成的 UTargetingFilterTask_SortByDistance：
 *   1) 后者的排序中心来自它自己 private 且非虚的 GetSourceLocation，只会用 SourceActor 的位置
 *      —— 那是"离施法者最近"，光标指着谁它不知道；
 *   2) 重写它的 GetScoreForTarget 会把父类实现整个绕过，却仍然继承
 *      bUseDistanceToNearestBlockingCollider / DistanceToCollisionChannel 两个可改但无效的开关，
 *      在数据驱动项目里就是"我改了值怎么没反应"的静默坑。
 * 继承 Base 只拿到排序机制本身（Execute 按 Score 排 + bAscending/bStableSort + debug 文本）。
 */
UCLASS()
class ARCANE_ARENA_API UArcaneTargetingSortTask_LockDistance : public UTargetingSortTask_Base
{
	GENERATED_BODY()
	
public:
	UArcaneTargetingSortTask_LockDistance(const FObjectInitializer& ObjectInitializer);
	
protected:
	//~ UTargetingSortTask_Base：Score 越小越靠前（升降序由父类 bAscending 决定）
	virtual float GetScoreForTarget(const FTargetingRequestHandle& TargetingHandle, const FTargetingDefaultResultData& TargetData) const override;
	
private:
	static FVector ResolveAimCenter(const FTargetingRequestHandle& TargetingHandle);
	
	// 瞄准点到目标碰撞体表面的 2D 距离平方；取不到表面点才退回 Actor 中心
	static float GetAimDistanceSquared(const FVector& AimCenter, AActor* TargetActor);
};
