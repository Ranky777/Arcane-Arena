// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArcaneUserWidget.h"
#include "ArcaneOwnerWidget.generated.h"

class AArcanePlayerState;

/** 归属实体与本机玩家的关系 */
UENUM(BlueprintType)
enum class EArcaneOwnerRelation : uint8
{
	Enemy,
	Self,
	Ally
};

/**
 * 有归属实体的世界内 UI（头顶血条等）。
 * 在纯 ASC 桥接基类之上叠加"我展示的是哪个 Actor"的上下文，
 * 视口 HUD 等无归属 UI 不应继承此类。
 *
 * 关系判定收口在 C++：TeamID 存在复制/赋值时序竞争（客户端 PS 未到、
 * 服务器 PostLogin 未写值），因此就绪前由 C++ 定时重试，就绪后一次性
 * 触发 OnOwnerRelationSet；蓝图只负责按结论选颜色。
 */
UCLASS()
class ARCANE_ARENA_API UArcaneOwnerWidget : public UArcaneUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Arcane|UI")
	void SetOwnerActor(AActor* InOwnerActor);

	UFUNCTION(BlueprintPure, Category = "Arcane|UI")
	AActor* GetOwnerActor() const { return OwnerActor; }

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Arcane|UI")
	TObjectPtr<AActor> OwnerActor;

	/** 关系就绪后触发一次（Self/Ally/Enemy）；数据未就绪期间不触发 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Arcane|UI")
	void OnOwnerRelationSet(EArcaneOwnerRelation Relation);

private:
	void TryResolveRelation();
	void ScheduleResolveRetry();

	FTimerHandle RelationRetryHandle;
	int32 RelationRetries = 0;

	// 0.2s * 100 = 最长重试 20s，防极端情况下无限重试
	static constexpr int32 MaxRelationRetries = 100;
};
