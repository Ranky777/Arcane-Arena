// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Online/ArcaneSessionSubsystem.h"
#include "ArcaneSessionRowWidget.generated.h"

class UButton;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnArcaneSessionRowClicked, const FArcaneSessionInfo&, Info, int32, SearchIndex);

/**
 * 房间列表行：C++ 负责填充数据与点击转发，WBP 只排版式。
 * 控件树（WBP_SessionRow）：Button_Row / Text_RoomName / Text_Players
 */
UCLASS()
class ARCANE_ARENA_API UArcaneSessionRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Arcane|UI")
	void SetupFromInfo(const FArcaneSessionInfo& InInfo);

	UFUNCTION(BlueprintCallable, Category = "Arcane|UI")
	void SetSelected(bool bSelected);

	UPROPERTY(BlueprintAssignable, Category = "Arcane|UI")
	FOnArcaneSessionRowClicked OnRowClicked;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Row;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_RoomName;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Players;

private:
	UFUNCTION()
	void HandleRowClicked();

	FArcaneSessionInfo CachedInfo;
	bool bSelected = false;
};
