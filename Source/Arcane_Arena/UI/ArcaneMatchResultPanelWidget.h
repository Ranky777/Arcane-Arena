// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ArcaneMatchResultPanelWidget.generated.h"

class UBorder;
class UButton;
class UPanelWidget;
class UTextBlock;

/**
 * 结算面板：胜负文案、比分、房主按钮权限全部在 C++。
 * 控件树（WBP_MatchResultPanel）：Text_Title / Text_Score / Text_Winner /
 * Button_Rematch / Button_BackToLobby / Button_Leave
 */
UCLASS()
class ARCANE_ARENA_API UArcaneMatchResultPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// HUD 在切到 MatchEnd 显示本面板时调用，保证内容是终局时刻的
	UFUNCTION(BlueprintCallable, Category = "Arcane|UI")
	void RefreshResult();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_Card;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> VerticalBox_Main;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Title;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Score;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Winner;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Rematch;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_BackToLobby;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Leave;

private:
	UFUNCTION()
	void HandleRematchClicked();

	UFUNCTION()
	void HandleBackToLobbyClicked();

	UFUNCTION()
	void HandleLeaveClicked();

private:
	// (RefreshResult 已提升为 public)
};
