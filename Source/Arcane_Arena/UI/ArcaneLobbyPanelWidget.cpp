// Fill out your copyright notice in the Description page of Project Settings.

#include "ArcaneLobbyPanelWidget.h"
#include "Components/Border.h"
#include "Framework/Text/TextLayout.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Framework/ArcaneGameState.h"
#include "Player/ArcanePlayerController.h"
#include "Player/ArcanePlayerState.h"
#include "GameFramework/PlayerState.h"
#include "UI/ArcaneUITheme.h"

void UArcaneLobbyPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (AArcaneGameState* GS = GetWorld()->GetGameState<AArcaneGameState>())
	{
		GS->OnRosterChanged.AddDynamic(this, &UArcaneLobbyPanelWidget::HandleRosterChanged);
	}

	if (Button_Team0)
	{
		Button_Team0->OnClicked.AddDynamic(this, &UArcaneLobbyPanelWidget::HandleTeam0Clicked);
	}
	if (Button_Team1)
	{
		Button_Team1->OnClicked.AddDynamic(this, &UArcaneLobbyPanelWidget::HandleTeam1Clicked);
	}
	if (Button_StartMatch)
	{
		Button_StartMatch->OnClicked.AddDynamic(this, &UArcaneLobbyPanelWidget::HandleStartClicked);
	}

	// —— 主题 ——
	if (Border_Backdrop)
	{
		Border_Backdrop->SetBrushColor(FLinearColor(0.02f, 0.03f, 0.05f, 0.62f));
	}
	if (Border_Card)
	{
		ArcaneUIStyle::ApplyPanel(Border_Card, ArcaneUITheme::CardBg, ArcaneUITheme::RadiusPanel,
			1.f, ArcaneUITheme::OutlineSoft);
		Border_Card->SetPadding(FMargin(30.f, 26.f));
	}
	ArcaneUIStyle::ApplyRowSpacing(VerticalBox_Main, ArcaneUITheme::PadRow);
	ArcaneUIStyle::ApplyButton(Button_Team0, ArcaneUITheme::TeamBlue, ArcaneUITheme::FontH2);
	ArcaneUIStyle::ApplyButton(Button_Team1, ArcaneUITheme::TeamRed, ArcaneUITheme::FontH2);
	ArcaneUIStyle::ApplyButton(Button_StartMatch, ArcaneUITheme::Success, ArcaneUITheme::FontH2);
	if (Text_Title)
	{
		Text_Title->SetText(NSLOCTEXT("ArcaneLobby", "Title", "选择阵营"));
		ArcaneUIStyle::ApplyTitle(Text_Title);
		Text_Title->SetJustification(ETextJustify::Center);
	}
	if (Text_Hint)
	{
		ArcaneUIStyle::ApplyText(Text_Hint, ArcaneUITheme::FontSmall, ArcaneUITheme::TextSecondary);
	}
	// 队名按钮里的计数文本换白（ApplyButton 已处理内容字号颜色）

	RefreshFromGameState();
}

void UArcaneLobbyPanelWidget::NativeDestruct()
{
	if (AArcaneGameState* GS = GetWorld()->GetGameState<AArcaneGameState>())
	{
		GS->OnRosterChanged.RemoveDynamic(this, &UArcaneLobbyPanelWidget::HandleRosterChanged);
	}

	if (Button_Team0)
	{
		Button_Team0->OnClicked.RemoveDynamic(this, &UArcaneLobbyPanelWidget::HandleTeam0Clicked);
	}
	if (Button_Team1)
	{
		Button_Team1->OnClicked.RemoveDynamic(this, &UArcaneLobbyPanelWidget::HandleTeam1Clicked);
	}
	if (Button_StartMatch)
	{
		Button_StartMatch->OnClicked.RemoveDynamic(this, &UArcaneLobbyPanelWidget::HandleStartClicked);
	}

	Super::NativeDestruct();
}

void UArcaneLobbyPanelWidget::HandleRosterChanged()
{
	RefreshFromGameState();
}

void UArcaneLobbyPanelWidget::RefreshFromGameState()
{
	const AArcaneGameState* GS = GetWorld()->GetGameState<AArcaneGameState>();
	if (GS == nullptr)
	{
		return;
	}

	int32 Count0 = 0;
	int32 Count1 = 0;
	for (const APlayerState* PS : GS->PlayerArray)
	{
		if (const AArcanePlayerState* ArcanePS = Cast<AArcanePlayerState>(PS))
		{
			if (ArcanePS->TeamID == 0) ++Count0;
			else if (ArcanePS->TeamID == 1) ++Count1;
		}
	}

	if (Text_Count0)
	{
		Text_Count0->SetText(FText::FromString(FString::Printf(TEXT("蓝队  %d 人"), Count0)));
	}
	if (Text_Count1)
	{
		Text_Count1->SetText(FText::FromString(FString::Printf(TEXT("红队  %d 人"), Count1)));
	}

	const AArcanePlayerState* LocalPS = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<AArcanePlayerState>() : nullptr;

	if (Button_StartMatch)
	{
		// 房主标记未复制到达前按非房主处理（宁可少显示按钮，不可让客户端越权）
		Button_StartMatch->SetVisibility(
			(LocalPS && LocalPS->IsRoomOwner()) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (Text_Hint)
	{
		if (LocalPS == nullptr || LocalPS->TeamID < 0)
		{
			Text_Hint->SetText(NSLOCTEXT("ArcaneUI", "LobbyWaitSync", "正在同步房间信息..."));
		}
		else if (LocalPS->IsRoomOwner())
		{
			Text_Hint->SetText(NSLOCTEXT("ArcaneUI", "LobbyHost", "你是房主：选人完成后点击开始比赛"));
		}
		else
	 {
			Text_Hint->SetText(NSLOCTEXT("ArcaneUI", "LobbyGuest", "已加入队伍，等待房主开始..."));
		}
	}
}

void UArcaneLobbyPanelWidget::HandleTeam0Clicked()
{
	if (AArcanePlayerController* PC = Cast<AArcanePlayerController>(GetOwningPlayer()))
	{
		PC->ServerRequestChangeTeam(0);
	}
}

void UArcaneLobbyPanelWidget::HandleTeam1Clicked()
{
	if (AArcanePlayerController* PC = Cast<AArcanePlayerController>(GetOwningPlayer()))
	{
		PC->ServerRequestChangeTeam(1);
	}
}

void UArcaneLobbyPanelWidget::HandleStartClicked()
{
	if (AArcanePlayerController* PC = Cast<AArcanePlayerController>(GetOwningPlayer()))
	{
		PC->ServerRequestStartMatch();
	}
}
