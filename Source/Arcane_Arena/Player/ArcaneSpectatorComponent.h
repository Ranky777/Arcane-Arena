// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ArcaneSpectatorComponent.generated.h"

class AArcaneCharacter;
class AArcanePlayerController;
/**
 * 本地观战组件。
 * 负责：死亡后选择存活队友、切人、目标失效校验、镜头切换。
 * 输入模式 / 键盘焦点仍由 PlayerController 负责。
 */
UCLASS(ClassGroup=(Arcane), meta=(BlueprintSpawnableComponent))
class ARCANE_ARENA_API UArcaneSpectatorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UArcaneSpectatorComponent();
	
	// 返回 false = 队里没有存活队友，不进入观战
	bool EnterSpectatorMode();
	
	void LeaveSpectatorMode();
	
	bool IsSpectating() const { return bIsSpectating; }
	
	void CycleNext();
	
	void CyclePrev();

private:
	AArcanePlayerController* GetOwnerPC() const;
	
	int32 GetLocalTeamID() const;
	
	void BuildLivingTeammates(TArray<AArcaneCharacter*>& OutTeammates) const;
	
	void ViewTeammate(AArcaneCharacter* Target);
	
	void CycleSpectateTarget(int32 Direction);
	
	void ValidateSpecateTarget();
	
	bool bIsSpectating = false;
	
	FTimerHandle SpectateValidateTimer;
	
	TWeakObjectPtr<AActor> CurrentSpectateTarget;
};
