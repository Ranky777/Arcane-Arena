// Fill out your copyright notice in the Description page of Project Settings.

#include "ArcaneOwnerWidget.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Player/ArcanePlayerState.h"
#include "TimerManager.h"

void UArcaneOwnerWidget::SetOwnerActor(AActor* InOwnerActor)
{
	if (OwnerActor == InOwnerActor)
	{
		return;
	}

	OwnerActor = InOwnerActor;

	// 新归属 → 重置重试状态并重新判定关系
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RelationRetryHandle);
	}
	RelationRetries = 0;
	TryResolveRelation();
}

void UArcaneOwnerWidget::TryResolveRelation()
{
	if (OwnerActor == nullptr)
	{
		return;
	}

	const UWorld* World = GetWorld();
	APlayerController* LocalPC = World ? World->GetFirstPlayerController() : nullptr;
	const AArcanePlayerState* LocalPS = LocalPC ? Cast<AArcanePlayerState>(LocalPC->PlayerState) : nullptr;
	const APawn* OwnerPawn = Cast<APawn>(OwnerActor);
	const AArcanePlayerState* OwnerPS = OwnerPawn ? Cast<AArcanePlayerState>(OwnerPawn->GetPlayerState()) : nullptr;

	// 未就绪：本机 PS / 归属 PS 缺失，或 TeamID 仍是"未知"哨兵（-1）→ 稍后重试。
	// 典型场景：客户端 PS/TeamID 复制未到达；服务器 PostLogin 尚未写入 TeamID。
	if (LocalPS == nullptr || OwnerPS == nullptr || LocalPS->TeamID < 0 || OwnerPS->TeamID < 0)
	{
		ScheduleResolveRetry();
		return;
	}

	EArcaneOwnerRelation Relation = EArcaneOwnerRelation::Enemy;
	if (OwnerActor == LocalPC->GetPawn())
	{
		Relation = EArcaneOwnerRelation::Self;
	}
	else if (OwnerPS->TeamID == LocalPS->TeamID)
	{
		Relation = EArcaneOwnerRelation::Ally;
	}

	OnOwnerRelationSet(Relation);
}

void UArcaneOwnerWidget::ScheduleResolveRetry()
{
	if (RelationRetries++ >= MaxRelationRetries)
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(RelationRetryHandle, this, &UArcaneOwnerWidget::TryResolveRelation, 0.2f, false);
	}
}
