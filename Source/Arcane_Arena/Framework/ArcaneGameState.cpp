// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneGameState.h"

#include "Net/UnrealNetwork.h"

AArcaneGameState::AArcaneGameState()
{
	TeamScores = {0, 0};
}

void AArcaneGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AArcaneGameState, MatchPhase);
	DOREPLIFETIME(AArcaneGameState, TeamScores);
	DOREPLIFETIME(AArcaneGameState, RoundNumber);
}

void AArcaneGameState::OnRep_Phase()
{
	// [PLACEHOLDER] Phase 6：回合阶段切换广播（开场/结束 UI 订阅点）
}
