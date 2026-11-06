// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneNetGuardComponent.h"


// Sets default values for this component's properties
UArcaneNetGuardComponent::UArcaneNetGuardComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	
	// 纯服务器信任边界：没有任何需要复制给客户端的字段。
	SetIsReplicated(false);
}

bool UArcaneNetGuardComponent::TryConsumeRpcBudget(FName Action, double Now, float Cost, float Capacity,
	float RefillPerSecond)
{
	if (Action.IsNone() || Cost <= 0.0f || Capacity <= 0.0f)
	{
		return false;
	}
	
	FRpcBudget& Budget = RpcBudgets.FindOrAdd(Action);
	if (Budget.LastRefillTime < 0.0f)
	{
		Budget.Tokens = Capacity;
		Budget.LastRefillTime = Now;
	}
	else
	{
		const double DeltaSeconds = FMath::Max(0.0, Now - Budget.LastRefillTime);
		Budget.Tokens = FMath::Min(Capacity, Budget.Tokens + static_cast<float>(DeltaSeconds) * RefillPerSecond);
		Budget.LastRefillTime = Now;
	}
	
	if (Budget.Tokens < Cost)
	{
		return false;
	}
	
	Budget.Tokens -= Cost;
	
	return true;
}

void UArcaneNetGuardComponent::StoreHomingRequest(FName AbilityID, AActor* ProposedTarget, const FVector& AimPoint,
	double Now)
{
	PendingHomingRequest.AbilityID = AbilityID;
	PendingHomingRequest.ProposedTarget = ProposedTarget;
	PendingHomingRequest.AimPoint = AimPoint;
	PendingHomingRequest.ServerTime = Now;
	bHasPendingHomingRequest = true;
}

bool UArcaneNetGuardComponent::ConsumeHomingRequest(FName ExpectedAbilityID, double Now, double MaxAge,
	FArcaneHomingRequest& OutRequest)
{
	if (!bHasPendingHomingRequest)
	{
		return false;
	}
	
	// 无论是否匹配都消费掉：一个待处理槽位只服务一次发射。
	bHasPendingHomingRequest = false;
	
	if (PendingHomingRequest.AbilityID != ExpectedAbilityID)
	{
		return false;
	}
	
	if (Now - PendingHomingRequest.ServerTime > MaxAge)
	{
		return false;
	}
	
	OutRequest = PendingHomingRequest;
	
	return true;
}

void UArcaneNetGuardComponent::ClearHomingRequest()
{
	bHasPendingHomingRequest = false;
}

void UArcaneNetGuardComponent::IssueHomingCommitToken(FName AbilityID, double Now)
{
	CommitTokenAbilityID = AbilityID;
	CommitTokenTime = Now;
	bHasCommitToken = true;
}

bool UArcaneNetGuardComponent::ConsumeHomingCommitToken(FName AbilityID, double Now, double MaxAge)
{
	if (!bHasCommitToken)
	{
		return false;
	}
	
	// 不是同一技能的 token 不要消费掉，留给它对应的 RPC。
	if (CommitTokenAbilityID != AbilityID)
	{
		return false;
	}
 
	if (Now - CommitTokenTime > MaxAge)
	{
		bHasCommitToken = false;
		return false;
	}
 
	bHasCommitToken = false;
	return true;
}
