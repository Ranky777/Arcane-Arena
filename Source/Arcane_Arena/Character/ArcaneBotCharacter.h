#pragma once

#include "CoreMinimal.h"
#include "Character/ArcaneCharacter.h"
#include "ArcaneBotCharacter.generated.h"

class UArcaneAttributeSet;

/**
 * Bot 专用角色基类：在此（且仅在此）构造 Character 端 ASC。
 * 玩家走 AArcaneCharacter + PlayerState 的 ASC，两者在类型层面分开，
 * 避免了"构造函数里读 EditDefaultsOnly 开关"失效的问题
 * （构造期 UPROPERTY 只有 C++ 默认值，BP 的 false 永远来不及生效）。
 */
UCLASS()
class ARCANE_ARENA_API AArcaneBotCharacter : public AArcaneCharacter
{
	GENERATED_BODY()

public:
	AArcaneBotCharacter();

	virtual UArcaneAbilitySystemComponent* GetArcaneASC() const override;

protected:
	virtual void BeginPlay() override;

private:
	// Bot 专用 ASC（仅本类构造；玩家类彻底没有这份组件）
	UPROPERTY(VisibleAnywhere, Category = "Arcane|GAS")
	TObjectPtr<UArcaneAbilitySystemComponent> ArcaneASC;

	// Bot 的属性集：必须与 ASC 同属一个 Owner，ASC 在 InitAbilityActorInfo 时自动发现。
	// 缺了它 GE_Damage 的 Health 修改器无处落地 → Bot 打不掉血、血条不动。
	UPROPERTY()
	TObjectPtr<UArcaneAttributeSet> ArcaneAttributeSet;
};
