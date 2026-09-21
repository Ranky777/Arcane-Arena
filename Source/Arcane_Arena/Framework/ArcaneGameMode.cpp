// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneGameMode.h"

#include "ArcaneGameState.h"
#include "Player/ArcaneHUD.h"
#include "Player/ArcanePlayerController.h"
#include "Player/ArcanePlayerState.h"

AArcaneGameMode::AArcaneGameMode()
{
	PlayerControllerClass = AArcanePlayerController::StaticClass();
	PlayerStateClass = AArcanePlayerState::StaticClass();
	GameStateClass = AArcaneGameState::StaticClass();
	HUDClass = AArcaneHUD::StaticClass();
	// DefaultPawnClass = nullptr;   // 先不管，由 BP_ArcaneGameMode 配置；避免误生成模板 Pawn
}

void AArcaneGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	
	if (AArcanePlayerState* PS = NewPlayer->GetPlayerState<AArcanePlayerState>())
	{
		// 交替分配队伍：第一名 → 0 队，第二名 → 1 队……
		// [PLACEHOLDER] Phase 6：Bot 补位与 3v3 平衡由回合系统接管
		const int32 Index = GameState->PlayerArray.Num() - 1; // 本玩家已计入数组
		PS->TeamID = (Index % 2 == 0) ? 0 : 1;
		
	}
}
