// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "ArcanePlayerState.generated.h"

class UArcaneAttributeSet;
class UArcaneAbilitySystemComponent;
/**
 * 
 */
UCLASS()
class ARCANE_ARENA_API AArcanePlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()
	
public:
	AArcanePlayerState();
	
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	
	UFUNCTION(BlueprintCallable)
	UArcaneAbilitySystemComponent* GetArcaneASC() const { return ArcaneASC; }
	
	UArcaneAttributeSet* GetArcaneAttributeSet() const { return AttributeSet; }
	
	// 3v3 队伍 ID（0/1），GameMode PostLogin 交替分配。
	// 默认 -1 = "未知/未分配"哨兵：客户端复制到达前、服务器 PostLogin 写入前
	// 都能被 UI 侧识别为"数据未就绪"，避免把默认 0 误当队 0（头顶血条染色的时序修复）。
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Team, Category = "Arcane|Match")
	int32 TeamID = -1;
	
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Arcane|Match")
	bool bIsDead = false;
	
	// 是否是房主（Listen Server 的本地玩家）。只有房主能开始比赛 / 再来一局 / 返回房间。
	// 客户端要读它来决定「开始游戏」按钮是否显示，所以必须复制。
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Arcane|Match")
	bool bIsRoomOwner = false;

	// 蓝图侧统一从这里读房主标记：Widget 拿到的是 Cast 后的 PlayerState，
	// 直接访问成员变量在蓝图里不好连线，包一层 Pure 更顺手。
	UFUNCTION(BlueprintPure, Category = "Arcane|Match")
	bool IsRoomOwner() const { return bIsRoomOwner; }

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	UFUNCTION()
	void OnRep_Team();
	
	UPROPERTY(VisibleAnywhere, Category = "Arcane|GAS")
	TObjectPtr<UArcaneAbilitySystemComponent> ArcaneASC;
	
	UPROPERTY()
	TObjectPtr<UArcaneAttributeSet> AttributeSet;
};
