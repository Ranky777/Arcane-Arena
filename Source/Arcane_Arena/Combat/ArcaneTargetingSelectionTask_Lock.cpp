// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneTargetingSelectionTask_Lock.h"

#include "ArcaneCombatFunctionLibrary.h"
#include "AI/ArcaneAIController.h"

UArcaneTargetingSelectionTask_Lock::UArcaneTargetingSelectionTask_Lock(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 默认值写在 C++ 而不是蓝图 CDO：消灭"CDO 一份、Preset 实例一份"的双份配置，
	// Preset 里只需要覆盖要偏离的那个技能
	SetShapeType(ETargetingAOEShape::Sphere);
	SetRadius(FScalableFloat(250.0f));
	CollisionProfileName.Name = TEXT("OverlapAllDynamic");
	bIgnoreSourceActor = true;
	bIgnoreInstigatorActor = true;
	bTraceComplex = false;
}

FVector UArcaneTargetingSelectionTask_Lock::GetSourceLocation_Implementation(
	const FTargetingRequestHandle& TargetingHandle) const
{
	const FVector Center = ResolveLockCenter(TargetingHandle);
	
	// 写回数据仓库：后面的排序任务读同一个值，圆心全程只算一次
	FTargetingSourceContext::FindOrAdd(TargetingHandle).SourceLocation = Center;
	
	return Center;
}

FVector UArcaneTargetingSelectionTask_Lock::ResolveLockCenter(const FTargetingRequestHandle& TargetingHandle) const
{
	const FTargetingSourceContext* Context = FTargetingSourceContext::Find(TargetingHandle);
	const AActor* Source = Context ? Context->SourceActor : nullptr;
	if (Source == nullptr)
	{
		return FVector::ZeroVector;
	}
	
	// 本地玩家：环套在鼠标指着的地面上
	const APawn* SourcePawn = Cast<APawn>(Source);
	if (SourcePawn != nullptr && SourcePawn->IsLocallyControlled())
	{
		if (const APlayerController* PC = Cast<APlayerController>(SourcePawn->GetController()))
		{
			FVector CursorPoint;
			if (UArcaneCombatFunctionLibrary::TryGetCursorGroundPoint(
				PC, Source, CursorTraceDistance, CursorFallbackDistance, CursorPoint))
			{
				return CursorPoint;
			}
		}
	}
	// Bot：服务器代管，没有光标可言 —— 环套在它的敌对目标身上
	if (SourcePawn != nullptr)
	{
		if (const AArcaneAIController* AI = Cast<AArcaneAIController>(SourcePawn->GetController()))
		{
			if (const AActor* Enemy = AI->ResolveEnemyTarget())
			{
				return Enemy->GetActorLocation();
			}
		}
	}
	// 兜底：自己脚下（观战、无目标、无控制器都走这条）
	return Source->GetActorLocation();
}
