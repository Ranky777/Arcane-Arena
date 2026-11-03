// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ArcaneCharacterMovementComponent.generated.h"

/**
 * 客户端预测用的 SavedMove：把冲刺状态一起存进移动快照。
 * 服务器纠正位置后，客户端会重放未确认的 Move；PrepMoveFor 负责把冲刺状态还原，
 * 否则重放会丢掉“这一帧正在冲刺”的事实，表现为纠偏后冲刺变短/瞬移。
 */
class FSavedMove_Arcane: public FSavedMove_Character
{
public:
	virtual void Clear() override;
	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character& ClientData) override;
	virtual void PrepMoveFor(ACharacter* C) override;
	virtual uint8 GetCompressedFlags() const override;
	
	bool bDashing = false;
	bool bWantsToDash = false;
	FVector DashDirection = FVector::ZeroVector;
	float DashSpeed = 0.0f;
	float DashTimeRemaining = 0.0f;
};

class FNetworkPredictionData_Client_Arcane : public FNetworkPredictionData_Client_Character
{
public:
	explicit FNetworkPredictionData_Client_Arcane(const UCharacterMovementComponent& ClientMovement)
		: FNetworkPredictionData_Client_Character(ClientMovement)
	{
	}
	
	virtual FSavedMovePtr AllocateNewMove() override
	{
		return FSavedMovePtr(new FSavedMove_Arcane());
	}
};

/**
 * 冲刺的客户端预测：
 * - 能力在本地立即置 bWantsToDash，下一次 CalcVelocity 直接给 Velocity，不等 RPC；
 * - 服务器跑同一份 GA（LocalPredicted 的服务器侧），由服务器权威推进；
 * - 客户端被纠正时，FSavedMove_Arcane 把冲刺状态带回重放，避免“纠偏把冲刺吃掉”。
 *
 * 注意：当前是地面冲刺。要空中冲刺需要额外处理 PhysFalling，
 * 因为普通下落路径不会调用 CalcVelocity。
 */
UCLASS()
class ARCANE_ARENA_API UArcaneCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UArcaneCharacterMovementComponent();
	
	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	virtual void Launch(FVector const& LaunchVel) override;
	
	// 能力入口：只登记“想冲刺”，方向在 CalcVelocity 里按当前输入/朝向解析。
	// 返回 false 表示已有一份冲刺在跑，调用方应结束能力。
	bool StartDash(float InSpeed, float InDuration);
	void StopDash();
	
	bool IsDashing() const { return bDashing; }
	bool WantsToDash() const { return bWantsToDash; }
	FVector GetDashDirection() const { return DashDirection; }
	float GetDashSpeed() const { return DashSpeed; }
	float GetDashDuration() const { return DashDuration; }
	float GetDashTimeRemaining() const { return DashTimeRemaining; }
	
	// 仅供 FSavedMove_Arcane 在客户端重放时还原状态
	void RestoreDashState(bool bInDashing, bool bInWantsToDash, const FVector& InDirection,
		float InSpeed, float InTimeRemaining);

protected:
	FVector ResolveDashDirection() const;
	
private:
	bool bDashing = false;
	bool bWantsToDash = false;
	FVector DashDirection = FVector::ZeroVector;
	float DashSpeed = 0.0f;
	float DashDuration = 0.0f;
	float DashTimeRemaining = 0.0f;
	
	// 默认纯水平冲刺；需要空中冲刺/保留重力时再打开
	UPROPERTY(EditDefaultsOnly, Category = "Arcane|Dash")
	bool bPreserveVerticalVelocity = false;
};
