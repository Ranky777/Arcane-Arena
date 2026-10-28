// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ArcaneMatchClient.generated.h"

UENUM(BlueprintType)
enum class EArcaneMatchCommand : uint8
{
	None = 0,
	ChangeTeam,
	StartMatch,
	Rematch,
	BackToLobby,
	AddBot,
	RemoveBot,
	BalanceBots,
	SetTeamCapacity,
	SetRoundsToWin
};

/**
 * UI → 本地 PlayerController 的比赛命令契约。
 * UI 不再 Cast 到 AArcanePlayerController；具体走哪个 Server RPC 由 PC 实现决定。
 */
UINTERFACE(MinimalAPI)
class UArcaneMatchClient : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class ARCANE_ARENA_API IArcaneMatchClient
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual void RequestMatchCommand(EArcaneMatchCommand Command, int32 Param = 0) = 0;
};
