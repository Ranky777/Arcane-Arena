// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneCharacterMovementComponent.h"

#include "Arcane_Arena.h"
#include "GameFramework/Character.h"


void FSavedMove_Arcane::Clear()
{
	FSavedMove_Character::Clear();
	
	bDashing = false;
	bWantsToDash = false;
	DashDirection = FVector::ZeroVector;
	DashSpeed = 0.0f;
	DashTimeRemaining = 0.0f;
}

void FSavedMove_Arcane::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel,
	class FNetworkPredictionData_Client_Character& ClientData)
{
	FSavedMove_Character::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);
	
	if (const UArcaneCharacterMovementComponent* CMC = C ? Cast<UArcaneCharacterMovementComponent>(C->GetCharacterMovement()) : nullptr)
	{
		bDashing = CMC->IsDashing();
		bWantsToDash = CMC->WantsToDash();
		DashDirection = CMC->GetDashDirection();
		DashSpeed = CMC->GetDashSpeed();
		DashTimeRemaining = CMC->GetDashTimeRemaining();
	}
	
	// 冲刺开始/结束的那一帧绝不与别的 Move 合并：
	// 合并会把“冲刺开始”这一帧的 DeltaTime 摊进旧 Move，重放时冲刺起点就错了。
	if (bDashing || bWantsToDash)
	{
		bForceNoCombine = true;
	}
}

void FSavedMove_Arcane::PrepMoveFor(ACharacter* C)
{
	FSavedMove_Character::PrepMoveFor(C);
	
	if (UArcaneCharacterMovementComponent* CMC = C ? Cast<UArcaneCharacterMovementComponent>(C->GetCharacterMovement()) : nullptr)
	{
		CMC->RestoreDashState(bDashing, bWantsToDash, DashDirection, DashSpeed, DashTimeRemaining);
	}
}

uint8 FSavedMove_Arcane::GetCompressedFlags() const
{
	uint8 Result = FSavedMove_Character::GetCompressedFlags();
	if (bDashing || bWantsToDash)
	{
		Result |= FLAG_Custom_0;
	}
	
	return Result;
}

UArcaneCharacterMovementComponent::UArcaneCharacterMovementComponent()
{
}

void UArcaneCharacterMovementComponent::CalcVelocity(float DeltaTime, float Friction, bool bFluid,
                                                     float BrakingDeceleration)
{
	// 起跑：此时 Acceleration 已由 MoveAutonomous / ReplicateMoveToServer 填好，
	// 所以客户端和服务器能从同一份输入里解析出同一个方向。
	if (bWantsToDash && !bDashing)
	{
		const FVector Direction = ResolveDashDirection();
		if (!Direction.IsNearlyZero())
		{
			DashDirection = Direction;
			DashTimeRemaining = DashDuration;
			bDashing = true;
			
			UE_LOG(LogArcane, Verbose, TEXT("[Dash] start dir=%s speed=%.0f dur=%.3f"),
				*DashDirection.ToCompactString(), DashSpeed, DashDuration);
		}
		
		bWantsToDash = false;
	}
	
	if (bDashing)
	{
		Velocity = DashDirection * DashSpeed;
		if (!bPreserveVerticalVelocity)
		{
			Velocity.Z = 0.0f;
		}
		
		DashTimeRemaining -= DeltaTime;
		if (DashTimeRemaining <= 0.0f)
		{
			DashTimeRemaining = 0.0f;
			bDashing = false;
			UE_LOG(LogArcane, Verbose, TEXT("[Dash] end"));
		}
		
		return;
	}
	
	Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
}

FNetworkPredictionData_Client* UArcaneCharacterMovementComponent::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		UArcaneCharacterMovementComponent* MutableThis = const_cast<UArcaneCharacterMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_Arcane(*this);
	}
	
	return ClientPredictionData;
}

void UArcaneCharacterMovementComponent::Launch(FVector const& LaunchVel)
{
	// 被击退/击飞时打断冲刺，否则下一帧 CalcVelocity 会把击退速度覆盖掉
	StopDash();
	Super::Launch(LaunchVel);
}

bool UArcaneCharacterMovementComponent::StartDash(float InSpeed, float InDuration)
{
	if (bDashing || bWantsToDash || InSpeed <= 0.0f || InDuration <= 0.0f)
	{
		return false;
	}
	
	DashSpeed = InSpeed;
	DashDuration = InDuration;
	DashTimeRemaining = InDuration;
	bWantsToDash = true;
	
	return true;
}

void UArcaneCharacterMovementComponent::StopDash()
{
	bDashing = false;
	bWantsToDash = false;
	DashTimeRemaining = 0.0f;
}

void UArcaneCharacterMovementComponent::RestoreDashState(bool bInDashing, bool bInWantsToDash,
	const FVector& InDirection, float InSpeed, float InTimeRemaining)
{
	bDashing = bInDashing;
	bWantsToDash = bInWantsToDash;
	DashDirection = InDirection;
	DashSpeed = InSpeed;
	DashTimeRemaining = InTimeRemaining;
	
	// 重放“冲刺开始”那一帧时，CMC 的 DashDuration 可能是上一次冲刺的旧值。
	// 起始帧的 DashTimeRemaining 就等于本次时长，所以直接用它覆盖。
	if (bInWantsToDash && !bInDashing)
	{
		DashDuration = FMath::Max(InTimeRemaining, 0.01f);
	}
}

FVector UArcaneCharacterMovementComponent::ResolveDashDirection() const
{
	if (CharacterOwner == nullptr)
	{
		return FVector::ZeroVector;
	}
	
	// 有移动输入就沿输入方向；站桩冲刺沿镜头朝向；再退化到角色前方
	FVector Direction = Acceleration;
	Direction.Z = 0.0f;
	
	if (Direction.IsNearlyZero())
	{
		Direction = CharacterOwner->GetControlRotation().Vector();
		Direction.Z = 0.0f;
	}
	
	if (Direction.IsNearlyZero())
	{
		Direction = CharacterOwner->GetActorForwardVector();
		Direction.Z = 0.0f;
	}
	
	return Direction.GetSafeNormal();
}
