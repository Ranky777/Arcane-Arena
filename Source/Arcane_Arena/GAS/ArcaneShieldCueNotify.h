#pragma once
 
#include "CoreMinimal.h"
#include "GameplayCueNotify_Actor.h"
#include "ArcaneShieldCueNotify.generated.h"
 
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMeshComponent;
 
/**
 * 护盾视觉 Cue：
 * - C++ 创建球体 + MID；OnActive 按目标胶囊尺寸贴合目标并淡入；
 * - OnRemove 轻微扩张淡出后结束（交还 CueManager 回收）；
 * - 蓝图子类 GC_Shield 只配材质/颜色/时长。
 */
UCLASS()
class ARCANE_ARENA_API AArcaneShieldCueNotify : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AArcaneShieldCueNotify();
	
	virtual bool OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	virtual bool WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	virtual bool OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Shield")
	TObjectPtr<UStaticMeshComponent> ShieldMesh;
	
	// 球体资产由 GC_Shield 的 Class Defaults 指定；C++ 不写死任何包路径
	UPROPERTY(EditDefaultsOnly, Category = "Shield")
	TObjectPtr<UStaticMesh> ShieldMeshAsset;
	
	UPROPERTY(EditDefaultsOnly, Category = "Shield")
	TObjectPtr<UMaterialInterface> ShieldMaterial;
	
	UPROPERTY(EditDefaultsOnly, Category = "Shield")
	FLinearColor ShieldColor = FLinearColor(0.35f, 0.78f, 1.0f, 1.0f);
	
	UPROPERTY(EditDefaultsOnly, Category = "Shield", meta = (ClampMin = "0.0"))
	float BaseOpacity = 0.35f;
 
	UPROPERTY(EditDefaultsOnly, Category = "Shield", meta = (ClampMin = "0.0"))
	float ShieldPadding = 1.12f;
 
	UPROPERTY(EditDefaultsOnly, Category = "Shield", meta = (ClampMin = "0.0"))
	float FadeInTime = 0.12f;
 
	UPROPERTY(EditDefaultsOnly, Category = "Shield", meta = (ClampMin = "0.0"))
	float FadeOutTime = 0.22f;
 
	UPROPERTY(EditDefaultsOnly, Category = "Shield", meta = (ClampMin = "0.0"))
	float PulseSpeed = 3.0f;
 
	UPROPERTY(EditDefaultsOnly, Category = "Shield", meta = (ClampMin = "0.0"))
	float PulseAmplitude = 0.10f;
	
public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
private:
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ShieldMID;
	
	FVector InitialScale = FVector::OneVector;
	float FadeAlpha = 0.0f;
	bool bFadingOut = false;
};
