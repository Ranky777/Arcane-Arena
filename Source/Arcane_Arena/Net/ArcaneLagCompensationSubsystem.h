// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArcaneLagCompensationSubsystem.generated.h"

class AArcaneCharacter;
/**
 * 服务器端近战延迟补偿。
 *
 * 只做一件事：记录角色胶囊的历史，按“客户端当时看到的时间”回退，
 * 再用线段扫掠体积 vs 历史胶囊做相交测试，产出合成 FHitResult。
 *
 * 边界：
 *  - 只回退目标胶囊，不回退攻击者武器骨骼；
 *  - 只服务近战角色目标，不处理投射物；
 *  - 主机/Bot 不回退；
 *  - 最大回退窗口 250ms。
 */
UCLASS()
class ARCANE_ARENA_API UArcaneLagCompensationSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;
	
	// 服务器：按 RewindSeconds 回退角色胶囊，做一次近战扫掠。
	// 每个角色最多返回一条 HitResult；调用方继续走原来的伤害/冲击管线。
	void SweepMeleeAgainstHistory(
		const AArcaneCharacter* Attacker,
		const FVector& Start,
		const FVector& End,
		float SweepRadius,
		float RewindSeconds,
		TArray<FHitResult>& OutHits) const;
	
	bool HasHistory(const AArcaneCharacter* Character) const;
	
	// 客户端 ping 的一半 + 插值缓冲；主机/Bot 返回 0。
	float ComputeRewindSeconds(const AArcaneCharacter* Attacker) const;
	
private:
	struct FArcaneCapsuleSnapshot
	{
		double ServerTime = 0.0;
		FVector Location = FVector::ZeroVector;
		FQuat Rotation = FQuat::Identity;
		float HalfHeight = 0.0f;
		float Radius = 0.0f;
	};
	
	struct FArcaneCapsuleHistory
	{
		TArray<FArcaneCapsuleSnapshot> Samples;
	};
	
	void RecordSnapshot(AArcaneCharacter* Character, double Now);
	bool SampleHistory(const AArcaneCharacter* Character, double RewindTime, FArcaneCapsuleSnapshot& OutSnapshot) const;
	void PruneHistory(double Now);
	
	TMap<AArcaneCharacter*, FArcaneCapsuleHistory> Histories;
	
	double LastRecordTime = 0.0;
	static constexpr double SampleInterval = 1.0 / 60.0;
	static constexpr double MaxHistorySeconds = 1.0;
};
