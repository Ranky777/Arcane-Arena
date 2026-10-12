// Fill out your copyright notice in the Description page of Project Settings.

#include "ArcaneMatchResultPanelWidget.h"
#include "Framework/Text/TextLayout.h"

#include "Blueprint/UserWidget.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Framework/ArcaneGameState.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Online/ArcaneSessionSubsystem.h"
#include "Player/ArcanePlayerController.h"
#include "Player/ArcanePlayerState.h"
#include "ArcaneUITheme.h"

void UArcaneMatchResultPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_Rematch)
	{
		Button_Rematch->OnClicked.AddDynamic(this, &UArcaneMatchResultPanelWidget::HandleRematchClicked);
	}
	if (Button_BackToLobby)
	{
		Button_BackToLobby->OnClicked.AddDynamic(this, &UArcaneMatchResultPanelWidget::HandleBackToLobbyClicked);
	}
	if (Button_Leave)
	{
		Button_Leave->OnClicked.AddDynamic(this, &UArcaneMatchResultPanelWidget::HandleLeaveClicked);
	}

	// —— 主题 ——
	if (Border_Card)
	{
		ArcaneUIStyle::ApplyPanel(Border_Card, ArcaneUITheme::CardBg, ArcaneUITheme::RadiusPanel,
			1.f, ArcaneUITheme::OutlineSoft);
		Border_Card->SetPadding(FMargin(34.f, 28.f));
	}
	ArcaneUIStyle::ApplyRowSpacing(VerticalBox_Main, 8.f);
	ArcaneUIStyle::ApplyButton(Button_Rematch, EArcaneButtonTone::Success, ArcaneUITheme::FontH2);
	ArcaneUIStyle::ApplyButton(Button_BackToLobby, EArcaneButtonTone::Primary, ArcaneUITheme::FontH2);
	ArcaneUIStyle::ApplyButton(Button_Leave, EArcaneButtonTone::Danger, ArcaneUITheme::FontH2);

	RefreshResult();
}

void UArcaneMatchResultPanelWidget::NativeDestruct()
{
	if (Button_Rematch)
	{
		Button_Rematch->OnClicked.RemoveDynamic(this, &UArcaneMatchResultPanelWidget::HandleRematchClicked);
	}
	if (Button_BackToLobby)
	{
		Button_BackToLobby->OnClicked.RemoveDynamic(this, &UArcaneMatchResultPanelWidget::HandleBackToLobbyClicked);
	}
	if (Button_Leave)
	{
		Button_Leave->OnClicked.RemoveDynamic(this, &UArcaneMatchResultPanelWidget::HandleLeaveClicked);
	}

	Super::NativeDestruct();
}

void UArcaneMatchResultPanelWidget::RefreshResult()
{
	const AArcaneGameState* GS = GetWorld()->GetGameState<AArcaneGameState>();
	if (GS == nullptr)
	{
		return;
	}

	const bool bWinnerIsLocal = GS->IsLocalTeamWinner();
	const int32 Score0 = GS->TeamScores.IsValidIndex(0) ? GS->TeamScores[0] : 0;
	const int32 Score1 = GS->TeamScores.IsValidIndex(1) ? GS->TeamScores[1] : 0;

	if (Text_Title)
	{
		Text_Title->SetText(bWinnerIsLocal
			? NSLOCTEXT("ArcaneUI", "Victory", "胜 利")
			: NSLOCTEXT("ArcaneUI", "Defeat", "惜 败"));
		ArcaneUIStyle::ApplyText(Text_Title, ArcaneUITheme::FontH1,
			bWinnerIsLocal ? ArcaneUITheme::TextGlow : ArcaneUITheme::TextSecondary);
		Text_Title->SetJustification(ETextJustify::Center);
	}
	if (Text_Score)
	{
		Text_Score->SetText(FText::FromString(FString::Printf(TEXT("%d : %d"), Score0, Score1)));
		ArcaneUIStyle::ApplyText(Text_Score, ArcaneUITheme::FontH2, ArcaneUITheme::TextPrimary);
		Text_Score->SetJustification(ETextJustify::Center);
	}
	if (Text_Winner)
	{
		const FText WinnerText = GS->MatchWinnerTeamID == 0
			? NSLOCTEXT("ArcaneUI", "WinnerBlue", "蓝队获胜")
			: NSLOCTEXT("ArcaneUI", "WinnerRed", "红队获胜");
		Text_Winner->SetText(WinnerText);
		const FLinearColor WinnerColor = GS->MatchWinnerTeamID == 0 ? ArcaneUITheme::TeamBlue : ArcaneUITheme::TeamRed;
		ArcaneUIStyle::ApplyText(Text_Winner, ArcaneUITheme::FontBody, WinnerColor);
		Text_Winner->SetJustification(ETextJustify::Center);
	}

	const AArcanePlayerState* LocalPS = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<AArcanePlayerState>() : nullptr;
	const bool bRoomOwner = LocalPS && LocalPS->IsRoomOwner();

	const ESlateVisibility HostVisibility = bRoomOwner ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	if (Button_Rematch)
	{
		Button_Rematch->SetVisibility(HostVisibility);
	}
	if (Button_BackToLobby)
	{
		Button_BackToLobby->SetVisibility(HostVisibility);
	}
}

void UArcaneMatchResultPanelWidget::HandleRematchClicked()
{
	if (AArcanePlayerController* PC = Cast<AArcanePlayerController>(GetOwningPlayer()))
	{
		PC->ServerRequestRematch();
	}
}

void UArcaneMatchResultPanelWidget::HandleBackToLobbyClicked()
{
	if (AArcanePlayerController* PC = Cast<AArcanePlayerController>(GetOwningPlayer()))
	{
		PC->ServerRequestBackToLobby();
	}
}

void UArcaneMatchResultPanelWidget::HandleLeaveClicked()
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
