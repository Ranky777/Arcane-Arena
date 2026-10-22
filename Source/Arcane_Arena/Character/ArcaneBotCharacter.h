#pragma once

#include "CoreMinimal.h"
#include "Character/ArcaneCharacter.h"
#include "ArcaneBotCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
/**
 * Bot 角色 = 纯类型/配置锚点。
 * Phase 9 身份统一后，ASC 与玩家一样归 PlayerState（Bot 的 bWantsPlayerState
 * 由 AArcaneAIController 打开），本类不持有任何 GAS 组件——
 * 基类 GetArcaneASC() 的 PlayerState 路径对人机是同一条规则。
 * 存在的意义：GameMode.BotCharacterClass 的类型约束 + BP_CombatEnemy 的
 * 配置隔离（动作/数值/DefaultAbilities 独立于玩家 BP 调整）。
 */
UCLASS()
class ARCANE_ARENA_API AArcaneBotCharacter : public AArcaneCharacter
{
	GENERATED_BODY()
	
public:
	AArcaneBotCharacter();

protected:
	virtual void BeginPlay() override;
	
	virtual void PostInitializeComponents() override;
	
	virtual FVector GetHomingAimOffset() const override;
	
	UPROPERTY(VisibleAnywhere, Category = "Arcane|Camera")
	TObjectPtr<USpringArmComponent> SpectateBoom;
	
	UPROPERTY(VisibleAnywhere, Category = "Arcane|Camera")
	TObjectPtr<UCameraComponent> SpectateCamera;
};
