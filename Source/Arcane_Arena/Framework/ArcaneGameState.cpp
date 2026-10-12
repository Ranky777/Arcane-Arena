// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneGameState.h"

#include "Net/UnrealNetwork.h"
#include "Player/ArcanePlayerState.h"

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
	DOREPLIFETIME(AArcaneGameState, MatchWinnerTeamID);
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

void AArcaneGameState::OnRep_WinnerTeam()
{
	// 胜方到达时 MatchPhase 几乎同时到达，两个 OnRep 顺序不保证。
	// 再广播一次 OnPhaseChanged，让结算面板无条件重新读一次两个值，最省事且不会漏。
	OnPhaseChanged.Broadcast(MatchPhase);
}

bool AArcaneGameState::IsLocalTeamWinner() const
{
	if (MatchWinnerTeamID == INDEX_NONE)
	{
		return false;
	}

	// 本地玩家队伍从 PlayerController 的 PlayerState 取（服务端与客户端一致）
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const AArcanePlayerState* PS = PC ? PC->GetPlayerState<AArcanePlayerState>() : nullptr;

	return PS != nullptr && PS->TeamID == MatchWinnerTeamID;
}
