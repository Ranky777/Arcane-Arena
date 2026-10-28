#include "Framework/ArcaneMatchFlowComponent.h"

#include "AI/ArcaneAIController.h"
#include "Arcane_Arena.h"
#include "Character/ArcaneCharacter.h"
#include "Framework/ArcaneGameMode.h"
#include "Framework/ArcaneGameState.h"
#include "Framework/ArcaneRosterComponent.h"
#include "GAS/ArcaneAbilitySystemComponent.h"
#include "GAS/ArcaneAttributeSet.h"
#include "GameFramework/PlayerController.h"
#include "Player/ArcanePlayerState.h"
#include "TimerManager.h"

UArcaneMatchFlowComponent::UArcaneMatchFlowComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

AArcaneGameMode* UArcaneMatchFlowComponent::GetOwnerGM() const
{
	return Cast<AArcaneGameMode>(GetOwner());
}

void UArcaneMatchFlowComponent::InitializeMatch()
{
	AArcaneGameMode* GM = GetOwnerGM();
	AArcaneGameState* GS = GetWorld() ? GetWorld()->GetGameState<AArcaneGameState>() : nullptr;
	if (GM == nullptr || GS == nullptr)
	{
		return;
	}

	if (!bPhaseChangedDelegateBound)
	{
		GS->OnPhaseChanged.AddDynamic(this, &UArcaneMatchFlowComponent::HandleMatchPhaseChanged);
		bPhaseChangedDelegateBound = true;
	}

	GS->MaxPlayersPerTeam = GM->GetMaxPlayersPerTeam();
	GS->MaxPlayersPerTeamHardCap = GM->RosterComponent
		? GM->RosterComponent->ComputeTeamCapacityCeiling()
		: FMath::Max(1, GM->GetMaxPlayersPerTeamHardCap());
	GS->RoundsToWin = GM->GetWinningScore();
	GS->RoundsToWinMax = FMath::Max(1, GM->GetWinningScoreHardCap());

	GS->MatchPhase = EArcaneMatchPhase::Warmup;
	GS->RoundNumber = 0;
	GS->TeamScores = {0, 0};
	GS->MatchWinnerTeamID = INDEX_NONE;

	GS->OnPhaseChanged.Broadcast(GS->MatchPhase);
	GS->OnRoundNumberChanged.Broadcast();
	GS->OnScoreChanged.Broadcast();
}

void UArcaneMatchFlowComponent::NotifyCharacterDeath(AArcaneCharacter* DeadCharacter)
{
	if (GetOwnerGM() == nullptr || DeadCharacter == nullptr)
	{
		return;
	}

	AArcaneGameState* GS = GetWorld() ? GetWorld()->GetGameState<AArcaneGameState>() : nullptr;
	if (GS == nullptr || GS->MatchPhase != EArcaneMatchPhase::Playing)
	{
		return;
	}

	if (AArcanePlayerState* PS = DeadCharacter->GetPlayerState<AArcanePlayerState>())
	{
		const int32 DeadTeam = PS->TeamID;
		if (IsTeamWiped(DeadTeam))
		{
			EndRound(DeadTeam == 0 ? 1 : 0);
		}
	}
}

void UArcaneMatchFlowComponent::LockAllCharacters(bool bLock)
{
	for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
	{
		if (AArcaneCharacter* Character = Cast<AArcaneCharacter>(It->Get()->GetPawn()))
		{
			Character->SetMatchLocked(bLock);
		}
	}
}

void UArcaneMatchFlowComponent::HandleMatchPhaseChanged(EArcaneMatchPhase NewPhase)
{
	LockAllCharacters(NewPhase != EArcaneMatchPhase::Playing);
}

void UArcaneMatchFlowComponent::TeleportCharacterToTeamStart(AArcaneCharacter* Character)
{
	AArcaneGameMode* GM = GetOwnerGM();
	if (GM == nullptr || Character == nullptr)
	{
		return;
	}

	AActor* Start = GM->ChoosePlayerStart(Character->GetController());
	if (Start != nullptr)
	{
		Character->TeleportTo(Start->GetActorLocation(), Start->GetActorRotation(), false, true);
	}
}

void UArcaneMatchFlowComponent::RespawnAllPlayers()
{
	AArcaneGameMode* GM = GetOwnerGM();
	AArcaneGameState* GS = GetWorld() ? GetWorld()->GetGameState<AArcaneGameState>() : nullptr;
	if (GM == nullptr || GS == nullptr)
	{
		return;
	}

	for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
	{
		AController* Controller = It->Get();
		if (Controller == nullptr)
		{
			continue;
		}

		if (Cast<APlayerController>(Controller) == nullptr && Cast<AArcaneAIController>(Controller) == nullptr)
		{
			continue;
		}

		if (APawn* OldPawn = Controller->GetPawn())
		{
			Controller->UnPossess();
			OldPawn->Destroy();
		}

		if (APlayerController* PC = Cast<APlayerController>(Controller))
		{
			PC->StartSpot = nullptr;
		}

		GM->RestartPlayer(Controller);
	}

	for (APlayerState* PS : GS->PlayerArray)
	{
		if (AArcanePlayerState* ArcPS = Cast<AArcanePlayerState>(PS))
		{
			ArcPS->bIsDead = false;

			if (UArcaneAbilitySystemComponent* ASC = ArcPS->GetArcaneASC())
			{
				const float MaxHealth = ASC->GetNumericAttribute(UArcaneAttributeSet::GetMaxHealthAttribute());
				ASC->SetNumericAttributeBase(UArcaneAttributeSet::GetHealthAttribute(), MaxHealth);

				const float MaxEnergy = ASC->GetNumericAttribute(UArcaneAttributeSet::GetMaxEnergyAttribute());
				ASC->SetNumericAttributeBase(UArcaneAttributeSet::GetEnergyAttribute(), MaxEnergy);

				ASC->ClearAllCooldowns();
			}
		}
	}
}

void UArcaneMatchFlowComponent::EnterPlayingPhase()
{
	AArcaneGameMode* GM = GetOwnerGM();
	AArcaneGameState* GS = GetWorld() ? GetWorld()->GetGameState<AArcaneGameState>() : nullptr;
	if (GM == nullptr || GS == nullptr)
	{
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(RoundEndTimerHandle);

	GS->TeamScores = {0, 0};
	GS->RoundNumber = 1;
	GS->MatchWinnerTeamID = INDEX_NONE;
	GS->MatchPhase = EArcaneMatchPhase::Playing;

	RespawnAllPlayers();
	LockAllCharacters(false);

	GS->OnScoreChanged.Broadcast();
	GS->OnRoundNumberChanged.Broadcast();
	GS->OnPhaseChanged.Broadcast(GS->MatchPhase);
}

void UArcaneMatchFlowComponent::EnterWarmupPhase()
{
	AArcaneGameMode* GM = GetOwnerGM();
	AArcaneGameState* GS = GetWorld() ? GetWorld()->GetGameState<AArcaneGameState>() : nullptr;
	if (GM == nullptr || GS == nullptr)
	{
		return;
	}

	if (GM->RosterComponent != nullptr)
	{
		GM->RosterComponent->DestroyAllBots();
	}

	GetWorld()->GetTimerManager().ClearTimer(RoundEndTimerHandle);

	GS->TeamScores = {0, 0};
	GS->RoundNumber = 0;
	GS->MatchWinnerTeamID = INDEX_NONE;
	GS->MatchPhase = EArcaneMatchPhase::Warmup;

	GS->OnScoreChanged.Broadcast();
	GS->OnRoundNumberChanged.Broadcast();
	GS->OnPhaseChanged.Broadcast(GS->MatchPhase);

	LockAllCharacters(true);
}

bool UArcaneMatchFlowComponent::IsTeamWiped(int32 TeamID) const
{
	const AArcaneGameState* GS = GetWorld() ? GetWorld()->GetGameState<AArcaneGameState>() : nullptr;
	if (GS == nullptr)
	{
		return false;
	}

	int32 TotalCount = 0;
	int32 AliveCount = 0;

	for (APlayerState* PS : GS->PlayerArray)
	{
		if (const AArcanePlayerState* ArcPS = Cast<AArcanePlayerState>(PS))
		{
			if (ArcPS->TeamID == TeamID)
			{
				TotalCount++;
				if (ArcPS->bIsDead == false)
				{
					AliveCount++;
				}
			}
		}
	}

	// 一个成员都没有的队不算团灭
	if (TotalCount == 0)
	{
		return false;
	}

	return AliveCount == 0;
}

void UArcaneMatchFlowComponent::EndRound(int32 WinnerTeamID)
{
	AArcaneGameMode* GM = GetOwnerGM();
	AArcaneGameState* GS = GetWorld() ? GetWorld()->GetGameState<AArcaneGameState>() : nullptr;
	if (GM == nullptr || GS == nullptr)
	{
		return;
	}

	GS->MatchPhase = EArcaneMatchPhase::RoundEnd;
	GS->OnPhaseChanged.Broadcast(GS->MatchPhase);

	if (GS->TeamScores.IsValidIndex(WinnerTeamID))
	{
		GS->TeamScores[WinnerTeamID]++;
		GS->OnScoreChanged.Broadcast();
	}

	GetWorld()->GetTimerManager().ClearTimer(RoundEndTimerHandle);

	const bool bMatchOver = GS->TeamScores.IsValidIndex(WinnerTeamID)
		&& GS->TeamScores[WinnerTeamID] >= GM->GetWinningScore();

	if (bMatchOver)
	{
		GS->MatchWinnerTeamID = WinnerTeamID;
		GS->MatchPhase = EArcaneMatchPhase::MatchEnd;
		GS->OnPhaseChanged.Broadcast(GS->MatchPhase);
		return;
	}

	GetWorld()->GetTimerManager().SetTimer(
		RoundEndTimerHandle,
		this,
		&UArcaneMatchFlowComponent::StartNextRound,
		GM->GetRoundEndDuration(),
		false);
}

void UArcaneMatchFlowComponent::StartNextRound()
{
	AArcaneGameState* GS = GetWorld() ? GetWorld()->GetGameState<AArcaneGameState>() : nullptr;
	if (GS == nullptr)
	{
		return;
	}

	RespawnAllPlayers();

	GS->MatchPhase = EArcaneMatchPhase::Playing;
	GS->MatchWinnerTeamID = INDEX_NONE;
	GS->RoundNumber++;

	GS->OnRoundNumberChanged.Broadcast();
	GS->OnPhaseChanged.Broadcast(GS->MatchPhase);
}