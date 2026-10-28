// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneSpectatorComponent.h"

#include "ArcanePlayerController.h"
#include "ArcanePlayerState.h"
#include "EngineUtils.h"
#include "Character/ArcaneCharacter.h"


// Sets default values for this component's properties
UArcaneSpectatorComponent::UArcaneSpectatorComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}

bool UArcaneSpectatorComponent::EnterSpectatorMode()
{
	if (bIsSpectating)
	{
		return true;
	}
	
	TArray<AArcaneCharacter*> Teammates;
	BuildLivingTeammates(Teammates);
	
	// 队里没活人（我方最后一名）→ 不进观战，原地等回合结算。
	if (Teammates.Num() == 0)
	{
		return false;
	}
	
	bIsSpectating = true;
	ViewTeammate(Teammates[0]);
	
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			SpectateValidateTimer,
			this,
			&UArcaneSpectatorComponent::ValidateSpecateTarget,
			0.25f,
			true
		);
	}
	
	return true;
}

void UArcaneSpectatorComponent::LeaveSpectatorMode()
{
	if (!bIsSpectating)
	{
		return;
	}
	
	bIsSpectating = false;
	
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpectateValidateTimer);
	}
	
	AArcanePlayerController* PC = GetOwnerPC();
	if (PC == nullptr)
	{
		return;
	}
	
	PC->OnSpectateTargetChanged.Broadcast(nullptr);
	
	// 交回自己的新身体（重生路径下 OnPossess 已 Possess 到新 Pawn）
	if (APawn* Self = PC->GetPawn())
	{
		PC->SetViewTargetWithBlend(Self, 0.4f);
	}
}

void UArcaneSpectatorComponent::CycleNext()
{
	CycleSpectateTarget(+1);
}

void UArcaneSpectatorComponent::CyclePrev()
{
	CycleSpectateTarget(-1);
}

AArcanePlayerController* UArcaneSpectatorComponent::GetOwnerPC() const
{
	return Cast<AArcanePlayerController>(GetOwner());	
}

int32 UArcaneSpectatorComponent::GetLocalTeamID() const
{
	if (const AArcanePlayerController* PC = GetOwnerPC())
	{
		if (const AArcanePlayerState* PS = PC->GetPlayerState<AArcanePlayerState>())
		{
			return PS->TeamID;
		}
	}
	
	return INDEX_NONE;
}

void UArcaneSpectatorComponent::BuildLivingTeammates(TArray<AArcaneCharacter*>& OutTeammates) const
{
	OutTeammates.Reset();
	
	const int32 MyTeam = GetLocalTeamID();
	UWorld* World = GetWorld();
	if (MyTeam == INDEX_NONE || World == nullptr)
	{
		return;
	}
 
	// 客户端只能靠复制下来的 Character.bIsDead / PlayerState.TeamID 枚举；
	// 远程玩家的 PlayerController 不复制，但 Pawn(Character) 是复制的，够用。
	for (TActorIterator<AArcaneCharacter> It(World); It; ++It)
	{
		AArcaneCharacter* Candidate = *It;
		if (Candidate == nullptr || Candidate->bIsDead)
		{
			continue;
		}
 
		const AArcanePlayerState* PS = Candidate->GetPlayerState<AArcanePlayerState>();
		if (PS == nullptr || PS->TeamID != MyTeam)
		{
			continue;
		}
 
		OutTeammates.Add(Candidate);
	}
}

void UArcaneSpectatorComponent::ViewTeammate(AArcaneCharacter* Target)
{
	if (Target == nullptr)
	{
		return;
	}
	
	AArcanePlayerController* PC = GetOwnerPC();
	if (PC == nullptr)
	{
		return;
	}
	
	CurrentSpectateTarget = Target;
	
	// BlendTime 0.4s 平滑切入，观战不突兀。
	PC->SetViewTargetWithBlend(Target, 0.4f);
	PC->OnSpectateTargetChanged.Broadcast(Target);
}

void UArcaneSpectatorComponent::CycleSpectateTarget(int32 Direction)
{
	if (!bIsSpectating)
	{
		return;
	}
 
	TArray<AArcaneCharacter*> Teammates;
	BuildLivingTeammates(Teammates);
	
	if (Teammates.Num() == 0)
	{
		return;
	}
 
	int32 CurrentIndex = 0;
	for (int32 i = 0; i < Teammates.Num(); ++i)
	{
		if (Teammates[i] == CurrentSpectateTarget.Get())
		{
			CurrentIndex = i;
			break;
		}
	}
 
	const int32 N = Teammates.Num();
	const int32 NextIndex = ((CurrentIndex + Direction) % N + N) % N; // 环绕
 
	ViewTeammate(Teammates[NextIndex]);
}

void UArcaneSpectatorComponent::ValidateSpecateTarget()
{
	TArray<AArcaneCharacter*> Teammates;
	BuildLivingTeammates(Teammates);
	for (AArcaneCharacter* T : Teammates)
	{
		if (T == CurrentSpectateTarget.Get())
		{
			return;
		}
	}
 
	// 目标已死/消失：有别的活人就推进到第一个
	if (Teammates.Num() > 0)
	{
		ViewTeammate(Teammates[0]);
		return;
	}
 
	// 全灭：保留看最后的尸体（若还有效）直到回合结束，不回切到自己尸体
	if (!CurrentSpectateTarget.IsValid())
	{
		LeaveSpectatorMode();
	}
}
