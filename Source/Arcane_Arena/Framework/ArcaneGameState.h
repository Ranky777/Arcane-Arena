// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "ArcaneGameState.generated.h"

UENUM(BlueprintType)
enum class EArcaneMatchPhase : uint8
{
	Warmup,
	Playing,
	RoundEnd,
	MatchEnd
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnArcaneScoreChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnArcanePhaseChanged, EArcaneMatchPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRoundNumberChanged);

/**
 * 
 */
UCLASS()
class ARCANE_ARENA_API AArcaneGameState : public AGameState
{
	GENERATED_BODY()
	
public:
	AArcaneGameState();
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Phase, Category = "Arcane|Match")
	EArcaneMatchPhase MatchPhase = EArcaneMatchPhase::Warmup;
	
	// [0]=队A, [1]=队B
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_TeamScores, Category = "Arcane|Match")
	TArray<int32> TeamScores;
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RoundNumber, Category = "Arcane|Match")
	int32 RoundNumber = 0;
	
	UPROPERTY(BlueprintAssignable, Category = "Arcane|Match")
	FOnArcaneScoreChanged OnScoreChanged;
	
	UPROPERTY(BlueprintAssignable, Category = "Arcane|Match")
	FOnArcanePhaseChanged OnPhaseChanged;
	
	UPROPERTY(BlueprintAssignable, Category = "Arcane|Match")
	FOnRoundNumberChanged OnRoundNumberChanged;
	
protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	UFUNCTION()
	void OnRep_Phase();
	
	UFUNCTION()
	void OnRep_TeamScores();
	
	UFUNCTION()
	void OnRep_RoundNumber();
};
