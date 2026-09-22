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
	OnPhaseChanged.Broadcast(MatchPhase);
}

void AArcaneGameState::OnRep_TeamScores()
{
	OnScoreChanged.Broadcast();
}

void AArcaneGameState::OnRep_RoundNumber()
{
	OnRoundNumberChanged.Broadcast();
}
