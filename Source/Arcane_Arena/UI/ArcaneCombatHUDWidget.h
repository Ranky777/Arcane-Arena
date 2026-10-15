// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/ArcaneAbilityTypes.h"
#include "UI/ArcaneUserWidget.h"
#include "Framework/ArcaneGameState.h"
#include "ArcaneCombatHUDWidget.generated.h"

class UArcaneLobbyPanelWidget;
class UArcaneMatchResultPanelWidget;
class UArcaneAbilitySystemComponent;
class UBorder;
class UImage;
class UPanelWidget;
class UProgressBar;
class UTextBlock;

/**
 * 战斗 HUD 总控：GameState 四委托、阶段显隐、比分/回合文本、能量条、
 * 三技能槽冷却全部收口在 C++；WBP_ArcaneHUD 只保留版式。
 *
 * 控件命名约定（迁移时在 WBP 里改名，缺失的可选绑定会自动降级）：
 * Group_ScoreBar / Group_AbilityBar / Panel_Lobby / Panel_Result /
 * Text_Team0Score / Text_Round / Text_Team1Score / Text_Phase / EnergyBar /
 * Icon_Dash|Secondary|Ultimate / Border_Dash|... / Text_CD_Dash|...
 */
UCLASS()
class ARCANE_ARENA_API UArcaneCombatHUDWidget : public UArcaneUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnConstructed() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeOnAttributeChanged(const FGameplayAttribute& Attribute, float NewValue, float OldValue) override;

	// —— 布局容器 ——
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> Group_ScoreBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> Group_AbilityBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_ScoreBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_AbilityBar;

	// —— 比分/阶段 ——
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Team0Score;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Round;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Team1Score;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Phase;

	// —— 能量条 ——
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> EnergyBar;

	// —— 子面板 ——
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UArcaneLobbyPanelWidget> Panel_Lobby;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UArcaneMatchResultPanelWidget> Panel_Result;

	// —— 技能槽：冲刺 ——
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Icon_Dash;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_Dash;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_CD_Dash;

	// —— 技能槽：副技能 ——
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Icon_Secondary;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_Secondary;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_CD_Secondary;

	// —— 技能槽：大招 ——
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Icon_Ultimate;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_Ultimate;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_CD_Ultimate;

private:
	struct FAbilitySlot
	{
		EArcaneAbilityInputID InputID = EArcaneAbilityInputID::None;
		UBorder* Dim = nullptr;
		UImage* Icon = nullptr;
		UTextBlock* CDText = nullptr;
	};

	UFUNCTION()
	void HandleScoreChanged();

	UFUNCTION()
	void HandleRoundChanged();

	UFUNCTION()
	void HandlePhaseChanged(EArcaneMatchPhase NewPhase);

	UFUNCTION()
	void HandleAbilityCommitted();

	void RefreshScores();
	void RefreshPhaseDisplay();
	void UpdateAbilitySlot(FAbilitySlot& Entry, float DeltaTime);
	// 加入客户端 PlayerState 复制晚于 HUD 构造：构造时绑不上就由 NativeTick 惰性重试
	bool TryBindLocalASC();
	UArcaneAbilitySystemComponent* GetArcaneASC() const;

	TArray<FAbilitySlot> Slots;

	float TargetEnergy = 1.0f;
	float CurrentEnergy = 1.0f;
	float MaxEnergySeen = 1.0f;

	// 冷却刚结束时做一次亮色回弹标记
	TMap<int32, float> SlotFlashTimers;
};
