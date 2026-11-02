// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/ArcaneOwnerWidget.h"
#include "ArcaneLifeBarWidget.generated.h"

class UImage;
class UProgressBar;
class UTextBlock;

/**
 * 头顶血条：Health/MaxHealth → 进度条的换算、按关系染色、低血脉冲全部在 C++。
 * 控件树（WBP_LifeBar）：Bar（ProgressBar）/（可选）Text_Health
 * 数据链路不变：AArcaneCharacter::InitHealthBar 调 InitFromASC + BindAttribute。
 */
UCLASS()
class ARCANE_ARENA_API UArcaneLifeBarWidget : public UArcaneOwnerWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeOnAttributeChanged(const FGameplayAttribute& Attribute, float NewValue, float OldValue) override;
	virtual void NativeOnOwnerRelationSet(EArcaneOwnerRelation Relation) override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> Bar;
	
	// 与血条共用同一根 Bar 的护盾段；WBP 里必须是 BarLayer(CanvasPanel) 的子控件
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> ShieldSegment;

	// 迁移期新增：血量数字（可选）
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Health;

private:
	float TargetFill = 1.0f;
	float CurrentFill = 1.0f;
	float MaxHealthSeen = 1.0f;
	bool bLowHealth = false;
	
	void UpdateShieldGeometry();
	
	float TargetShield = 0.0f;
	float CurrentShield = 0.0f;
	float CachedShieldRadius = -1.0f;
	bool bShieldRoundedLeft = false;
};
