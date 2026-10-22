// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneTargetingSortTask_LockDistance.h"

UArcaneTargetingSortTask_LockDistance::UArcaneTargetingSortTask_LockDistance(
	const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	// 默认"离圆心最近优先"，Preset 里要反序（最远优先）就覆盖 bAscending
	bAscending = true;
	bStableSort = true;
}

float UArcaneTargetingSortTask_LockDistance::GetScoreForTarget(const FTargetingRequestHandle& TargetingHandle,
                                                               const FTargetingDefaultResultData& TargetData) const
{
	AActor* Target = TargetData.HitResult.GetActor();
	if (Target == nullptr)
	{
		return 0.0f;
	}
	
	return GetAimDistanceSquared(ResolveAimCenter(TargetingHandle), Target);
}

FVector UArcaneTargetingSortTask_LockDistance::ResolveAimCenter(const FTargetingRequestHandle& TargetingHandle)
{
	if (const FTargetingSourceContext* Context = FTargetingSourceContext::Find(TargetingHandle))
	{
		return Context->SourceLocation;
	}
	
	return FVector::ZeroVector;
}

float UArcaneTargetingSortTask_LockDistance::GetAimDistanceSquared(const FVector& AimCenter, AActor* TargetActor)
{
	// 表面最近点：两个"中心差不多远"的人，靠体型分胜负不合理
	FVector NearestOnCollider;
	const float DistanceToCollider = TargetActor->ActorGetDistanceToCollision(AimCenter, ECC_Pawn, NearestOnCollider);
	if (DistanceToCollider >= 0.0f)
	{
		const FVector Flat = NearestOnCollider - AimCenter;
		return FVector(Flat.X, Flat.Y, 0.0f).SizeSquared();
	}
	
	const FVector Flat = TargetActor->GetActorLocation() - AimCenter;
	return FVector(Flat.X, Flat.Y, 0.0f).SizeSquared();
}
