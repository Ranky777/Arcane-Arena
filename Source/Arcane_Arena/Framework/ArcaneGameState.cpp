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
	DOREPLIFETIME(AArcaneGameState, RosterVersion);
	DOREPLIFETIME(AArcaneGameState, MaxPlayersPerTeam);
	DOREPLIFETIME(AArcaneGameState, MaxPlayersPerTeamHardCap);
	DOREPLIFETIME(AArcaneGameState, RoundsToWin);
	DOREPLIFETIME(AArcaneGameState, RoundsToWinMax);
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

void AArcaneGameState::OnRep_RosterVersion()
{
	OnRosterChanged.Broadcast();
}

void AArcaneGameState::OnRep_TeamCapacity()
{
	// 上限变了但名单可能没变（房主只是把 3 改成 4），所以这里自己广播一次，
	// 不依赖 RosterVersion 到达顺序。刷新函数是纯读取，多广播一次没有副作用。
	OnRosterChanged.Broadcast();
}

void AArcaneGameState::OnRep_MatchRules()
{
	// 同上：赛制变化不涉及名单，必须自己驱动一次面板刷新
	OnRosterChanged.Broadcast();
}

void AArcaneGameState::NotifyRosterChanged()
{
	if (GetLocalRole() == ROLE_Authority)
	{
		RosterVersion++;
		OnRosterChanged.Broadcast();
	}
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
