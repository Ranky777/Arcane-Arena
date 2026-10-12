// Fill out your copyright notice in the Description page of Project Settings.

#include "ArcanePauseMenuWidget.h"
#include "Framework/Text/TextLayout.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Online/ArcaneSessionSubsystem.h"
#include "Player/ArcanePlayerController.h"
#include "ArcaneUITheme.h"

void UArcanePauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_Resume)
	{
		Button_Resume->OnClicked.AddDynamic(this, &UArcanePauseMenuWidget::HandleResumeClicked);
	}
	if (Button_ReturnToLobby)
	{
		Button_ReturnToLobby->OnClicked.AddDynamic(this, &UArcanePauseMenuWidget::HandleReturnToLobbyClicked);
	}
	if (Button_Quit)
	{
		Button_Quit->OnClicked.AddDynamic(this, &UArcanePauseMenuWidget::HandleQuitClicked);
	}

	// —— 主题 ——
	if (Border_Backdrop)
	{
		Border_Backdrop->SetBrushColor(FLinearColor(0.01f, 0.015f, 0.03f, 0.72f));
	}
	if (Border_Card)
	{
		ArcaneUIStyle::ApplyPanel(Border_Card, ArcaneUITheme::CardBg, ArcaneUITheme::RadiusPanel,
			1.f, ArcaneUITheme::OutlineSoft);
		Border_Card->SetPadding(FMargin(30.f, 26.f));
	}
	if (Text_Title)
	{
		ArcaneUIStyle::ApplyTitle(Text_Title, ArcaneUITheme::FontH2);
		Text_Title->SetJustification(ETextJustify::Center);
	}
	ArcaneUIStyle::ApplyButton(Button_Resume, EArcaneButtonTone::Primary, ArcaneUITheme::FontH2);
	ArcaneUIStyle::ApplyButton(Button_ReturnToLobby, EArcaneButtonTone::Secondary, ArcaneUITheme::FontH2);
	ArcaneUIStyle::ApplyButton(Button_Quit, EArcaneButtonTone::Danger, ArcaneUITheme::FontH2);
}

void UArcanePauseMenuWidget::NativeDestruct()
{
	if (Button_Resume)
	{
		Button_Resume->OnClicked.RemoveDynamic(this, &UArcanePauseMenuWidget::HandleResumeClicked);
	}
	if (Button_ReturnToLobby)
	{
		Button_ReturnToLobby->OnClicked.RemoveDynamic(this, &UArcanePauseMenuWidget::HandleReturnToLobbyClicked);
	}
	if (Button_Quit)
	{
		Button_Quit->OnClicked.RemoveDynamic(this, &UArcanePauseMenuWidget::HandleQuitClicked);
	}

	Super::NativeDestruct();
}

void UArcanePauseMenuWidget::HandleResumeClicked()
{
	if (AArcanePlayerController* PC = Cast<AArcanePlayerController>(GetOwningPlayer()))
	{
		PC->ClosePauseMenu();
	}
}

void UArcanePauseMenuWidget::HandleReturnToLobbyClicked()
{
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (UArcaneSessionSubsystem* Sub = GI->GetSubsystem<UArcaneSessionSubsystem>())
		{
			Sub->ReturnToLobby();
			return;
		}
	}
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UArcanePauseMenuWidget::HandleQuitClicked()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}
