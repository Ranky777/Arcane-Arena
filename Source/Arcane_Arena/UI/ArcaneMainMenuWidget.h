// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ArcaneMainMenuWidget.generated.h"

class UArcaneSessionRowWidget;
class UArcaneSessionSubsystem;
class UArcaneSettingsWidget;
class UBorder;
class UButton;
class UPanelWidget;
class UScrollBox;
class UTextBlock;
struct FArcaneSessionInfo;

/**
 * 主菜单：会话搜索/创建/加入的全部逻辑在 C++。
 * 控件树（WBP_MainMenu）：Button_Create / Button_Refresh / Button_Join /
 * ScrollBox_Sessions /（可选）Text_Status
 */
UCLASS()
class ARCANE_ARENA_API UArcaneMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Create;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Refresh;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Join;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Settings;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_Backdrop;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_Card;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> Panel_Main;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Title;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> ScrollBox_Sessions;

	// 迁移期新增：状态提示行（没有也不影响逻辑）
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Status;

	UPROPERTY(EditDefaultsOnly, Category = "Arcane|UI")
	TSubclassOf<UArcaneSessionRowWidget> SessionRowClass;

	// 设置面板（WBP_Settings，父类 UArcaneSettingsWidget），在 Defaults 里指认
	UPROPERTY(EditDefaultsOnly, Category = "Arcane|UI")
	TSubclassOf<UArcaneSettingsWidget> SettingsWidgetClass;

private:
	UFUNCTION()
	void HandleSessionsFound(const TArray<FArcaneSessionInfo>& Sessions);

	UFUNCTION()
	void HandleCreateClicked();

	UFUNCTION()
	void HandleRefreshClicked();

	UFUNCTION()
	void HandleJoinClicked();

	UFUNCTION()
	void HandleRowClicked(const FArcaneSessionInfo& Info, int32 SearchIndex);

	UFUNCTION()
	void HandleSettingsClicked();

	void RebuildSessionList(const TArray<FArcaneSessionInfo>& Sessions);
	void SetStatus(const FString& Text);
	UArcaneSessionSubsystem* GetSessionSubsystem() const;

	UPROPERTY()
	TObjectPtr<UArcaneSettingsWidget> SettingsPanel;

	int32 SelectedSearchIndex = INDEX_NONE;
	TArray<TObjectPtr<UArcaneSessionRowWidget>> SpawnedRows;
};
