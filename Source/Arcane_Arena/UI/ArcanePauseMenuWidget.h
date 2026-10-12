// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ArcanePauseMenuWidget.generated.h"

class UBorder;
class UButton;
class UTextBlock;

/**
 * 暂停菜单：继续/回主菜单/退出 逻辑全部在 C++。
 * 控件树（WBP_PauseMenu）：Button_Resume / Button_ReturnToLobby / Button_Quit
 */
UCLASS()
class ARCANE_ARENA_API UArcanePauseMenuWidget : public UUserWidget
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
	TObjectPtr<UTextBlock> Text_Title;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Resume;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_ReturnToLobby;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Quit;

private:
	UFUNCTION()
	void HandleResumeClicked();

	UFUNCTION()
	void HandleReturnToLobbyClicked();

	UFUNCTION()
	void HandleQuitClicked();
};
