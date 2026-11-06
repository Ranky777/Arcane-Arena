// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ArcaneNetGuardComponent.generated.h"

/**
 * 客户端请求的“信任边界”组件，挂在 AArcanePlayerState 上，随 PlayerState 跨重生存活。
 *
 * 职责只有两件：
 * 1) 令牌桶：限制单个客户端某类 Server RPC 的调用频率，防止可靠 RPC 刷包。
 * 2) 意图缓存：远程技能 RPC 只登记“我想打哪里/想打谁”，不生成弹体；
 *    服务器 GA 在 CommitAbility 成功之后消费它，保证冷却/耗能永远是发射前的门禁。
 *
 * 它不做伤害结算、不做目标合法性判断（那仍然在 Combat 层）。
 */
struct FArcaneHomingRequest
{
	FName AbilityID = NAME_None;
	TWeakObjectPtr<AActor> ProposedTarget;
	FVector AimPoint = FVector::ZeroVector;
	double ServerTime = 0.0f;
};

UCLASS(ClassGroup=(Arcane), meta=(BlueprintSpawnableComponent))
class ARCANE_ARENA_API UArcaneNetGuardComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UArcaneNetGuardComponent();
	
	// 令牌桶。返回 false 表示本次请求超限，调用方应拒绝。
	bool TryConsumeRpcBudget(FName Action, double Now, float Cost = 1.0f, float Capacity = 3.0f, float RefillPerSecond = 2.0f);
	
	// 只登记意图；不生成弹体。
	void StoreHomingRequest(FName AbilityID, AActor* ProposedTarget, const FVector& AimPoint, double Now);
	
	// 服务器 GA 在 CommitAbility 之后调用。最多消费一次；过期或 AbilityID 不匹配都返回 false。
	bool ConsumeHomingRequest(FName ExpectedAbilityID, double Now, double MaxAge, FArcaneHomingRequest& OutRequest);
	
	void ClearHomingRequest();
	
	bool HasPendingHomingRequest() const { return bHasPendingHomingRequest; }
	
	// 调试计数：后续做网络诊断 HUD 时直接读。
	int32 AcceptedHomingRequests = 0;
	int32 RejectedHomingRequests = 0;
	
	// 服务器 GA 在 CommitAbility 成功后发一张短时“发射许可”。
	// RPC 只有拿到许可才允许发射；许可与 AbilityID 绑定、一次性。
	void IssueHomingCommitToken(FName AbilityID, double Now);
	bool ConsumeHomingCommitToken(FName AbilityID, double Now, double MaxAge);
	bool HasHomingCommitToken() const { return bHasCommitToken; }
	
private:
	struct FRpcBudget
	{
		float Tokens = 0.0f;
		double LastRefillTime = -1.0f;
	};
	
	TMap<FName, FRpcBudget> RpcBudgets;
	
	FArcaneHomingRequest PendingHomingRequest;
	bool bHasPendingHomingRequest = false;
	
	FName CommitTokenAbilityID = NAME_None;
	double CommitTokenTime = 0.0f;
	bool bHasCommitToken = false;
};
