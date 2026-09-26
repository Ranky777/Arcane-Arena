#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ArcaneCombatData.generated.h"

class UGameplayEffect;

UENUM(BlueprintType)
enum class EArcaneTargetingMode : uint8
{
	MeleeSweep, // 近战扇形/胶囊扫
	Projectile, // 远程弹道
	AOE         // 范围
};

UENUM(BlueprintType)
enum class EArcaneDisplacementKind : uint8
{
	None,
	Knockback, // 击退（贴地横推）
	Knockup, // 击飞（带滞空）
	Pull // 拉拽
};

UENUM(BlueprintType)
enum class EArcaneDirectionMode : uint8
{
	None,
	AwayFromSource, // 沿"攻击者→目标"反方向
	AlongHitNormal, // 沿命中表面法线（近战标准）
	Fixed           // 固定世界方向（配置里给向量，未来用）
};

USTRUCT(BlueprintType)
struct FArcaneAbilityRow : public FTableRowBase
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float BaseDamage = 0.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName DamageFalloffCurve; // CurveTable行名
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float EnergyCost = 0.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float CooldownDuration = 0.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag CooldownTag; // 共享冷却容器标签
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EArcaneTargetingMode TargetingMode = EArcaneTargetingMode::MeleeSweep;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Range = 200.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Radius = 30.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UGameplayEffect> DamageEffectClass;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName ImpactProfileID; // ImpactTable行名
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTagContainer AbilityTags;
};

USTRUCT(BlueprintType)
struct FArcaneImpactRow : public FTableRowBase
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EArcaneDisplacementKind DisplacementKind = EArcaneDisplacementKind::None;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EArcaneDirectionMode DirectionMode = EArcaneDirectionMode::None;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float HorizontalImpulse = 0.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float VerticalImpulse = 0.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float DisplacementDuration = 0.0f; // 击飞滞空/位移状态时长
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName FalloffCurve; // 距离衰减曲线
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTagContainer StatusTags; // Stun / Root / Slow
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float StatusDuration = 0.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float HitPause = 0.0f; // 命中顿帧（秒）
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag HitReactionTag; // 受击动画
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTagContainer GameplayCueTags; // VFX/SFX
};

UCLASS(BlueprintType)
class UArcaneCombatData :public UDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tables")
	TSoftObjectPtr<UDataTable> AbilityTable;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tables")
	TSoftObjectPtr<UDataTable> ImpactTable;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tables")
	TSoftObjectPtr<UCurveTable> FalloffCurves;
	
	// 全局单例：在 GameInstance::Init 里 LoadObject 后赋值，库函数直接读它
	static const UArcaneCombatData* Get();
	static void SetRegistry(const UArcaneCombatData* InRegistry) { Singleton = InRegistry; }
	
private:
	static const UArcaneCombatData* Singleton;
};

