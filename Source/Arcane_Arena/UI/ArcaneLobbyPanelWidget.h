// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ArcaneLobbyPanelWidget.generated.h"

class UBorder;
class UButton;
class UPanelWidget;
class UTextBlock;

/**
 * 选阵营面板：人数实时统计、房主权限显隐、换队/开始 RPC 全部在 C++。
 * 控件树（WBP_LobbyPanel）：Button_Team0 / Button_Team1 / Button_StartMatch /
 * Text_Hint / Text_Count0 / Text_Count1（原 TextBlock_693 / TextBlock_762，迁移时改名）
 */
UCLASS()
class ARCANE_ARENA_API UArcaneLobbyPanelWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_Backdrop;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_Card;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> VerticalBox_Main;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Team0;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Team1;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_StartMatch;
	
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_RemoveBot0;
	
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_RemoveBot1;
	
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_AddBot0;
	
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_AddBot1;
	
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_BalanceBots;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Title;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Hint;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Count0;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Count1;

private:
	UFUNCTION()
	void HandleRosterChanged();

	UFUNCTION()
	void HandleTeam0Clicked();

	UFUNCTION()
	void HandleTeam1Clicked();

	UFUNCTION()
	void HandleStartClicked();

	void RefreshFromGameState();
	
	UFUNCTION()
	void HandleAddBot0Clicked();
	
	UFUNCTION()
	void HandleAddBot1Clicked();
	
	UFUNCTION()
	void HandleRemoveBot0Clicked();
	
	UFUNCTION()
	void HandleRemoveBot1Clicked();
	
	UFUNCTION()
	void HandleBalanceClicked();
};
