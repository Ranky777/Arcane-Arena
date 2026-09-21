// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "ArcaneGameState.generated.h"

UENUM(BlueprintType)
enum class EArcaneMatchPhase : uint8
{
	Warmup,
	Playing,
	RoundEnd,
	MatchEnd
};

/**
 * 
 */
UCLASS()
class ARCANE_ARENA_API AArcaneGameState : public AGameStateBase
{
	GENERATED_BODY()
	
public:
	AArcaneGameState();
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Phase, Category = "Arcane|Match")
	EArcaneMatchPhase MatchPhase = EArcaneMatchPhase::Warmup;
	
	// [0]=队A, [1]=队B
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Arcane|Match")
	TArray<int32> TeamScores;
	
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Arcane|Match")
	int32 RoundNumber = 0;
	
protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	UFUNCTION()
	void OnRep_Phase();
};
