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
	if (Button_AddBot0)
	{
		Button_AddBot0->OnClicked.AddDynamic(this, &UArcaneLobbyPanelWidget::HandleAddBot0Clicked);
	}
	if (Button_RemoveBot0)
	{
		Button_RemoveBot0->OnClicked.AddDynamic(this, &UArcaneLobbyPanelWidget::HandleRemoveBot0Clicked);
	}
	if (Button_AddBot1)
	{
		Button_AddBot1->OnClicked.AddDynamic(this, &UArcaneLobbyPanelWidget::HandleAddBot1Clicked);
	}
	if (Button_RemoveBot1)
	{
		Button_RemoveBot1->OnClicked.AddDynamic(this, &UArcaneLobbyPanelWidget::HandleRemoveBot1Clicked);
	}
	if (Button_BalanceBots)
	{
		Button_BalanceBots->OnClicked.AddDynamic(this, &UArcaneLobbyPanelWidget::HandleBalanceClicked);
	}
	if (Button_CapMinus)
	{
		Button_CapMinus->OnClicked.AddDynamic(this, &UArcaneLobbyPanelWidget::HandleCapMinusClicked);
	}
	if (Button_CapPlus)
	{
		Button_CapPlus->OnClicked.AddDynamic(this, &UArcaneLobbyPanelWidget::HandleCapPlusClicked);
	}
	if (Button_RoundsMinus)
	{
		Button_RoundsMinus->OnClicked.AddDynamic(this, &UArcaneLobbyPanelWidget::HandleRoundsMinusClicked);
	}
	if (Button_RoundsPlus)
	{
		Button_RoundsPlus->OnClicked.AddDynamic(this, &UArcaneLobbyPanelWidget::HandleRoundsPlusClicked);
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
	ArcaneUIStyle::ApplyButton(Button_AddBot0, ArcaneUITheme::TeamBlue, ArcaneUITheme::FontSmall);
	ArcaneUIStyle::ApplyButton(Button_RemoveBot0, ArcaneUITheme::TeamBlue, ArcaneUITheme::FontSmall);
	ArcaneUIStyle::ApplyButton(Button_AddBot1, ArcaneUITheme::TeamRed, ArcaneUITheme::FontSmall);
	ArcaneUIStyle::ApplyButton(Button_RemoveBot1, ArcaneUITheme::TeamRed, ArcaneUITheme::FontSmall);
	ArcaneUIStyle::ApplyButton(Button_BalanceBots, ArcaneUITheme::CardBg, ArcaneUITheme::FontSmall);
	ArcaneUIStyle::ApplyButton(Button_CapMinus, ArcaneUITheme::CardBg, ArcaneUITheme::FontSmall);
	ArcaneUIStyle::ApplyButton(Button_CapPlus, ArcaneUITheme::CardBg, ArcaneUITheme::FontSmall);
	ArcaneUIStyle::ApplyButton(Button_RoundsMinus, ArcaneUITheme::CardBg, ArcaneUITheme::FontSmall);
	ArcaneUIStyle::ApplyButton(Button_RoundsPlus, ArcaneUITheme::CardBg, ArcaneUITheme::FontSmall);
	
	if (Text_Capacity)
	{
		ArcaneUIStyle::ApplyText(Text_Capacity, ArcaneUITheme::FontSmall, ArcaneUITheme::TextSecondary);
	}
	if (Text_Rounds)
	{
		ArcaneUIStyle::ApplyText(Text_Rounds, ArcaneUITheme::FontSmall, ArcaneUITheme::TextSecondary);
	}
	
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
	if (Button_AddBot0)
	{
		Button_AddBot0->OnClicked.RemoveDynamic(this, &UArcaneLobbyPanelWidget::HandleAddBot0Clicked);
	}
	if (Button_RemoveBot0)
	{
		Button_RemoveBot0->OnClicked.RemoveDynamic(this, &UArcaneLobbyPanelWidget::HandleRemoveBot0Clicked);
	}
	if (Button_AddBot1)
	{
		Button_AddBot1->OnClicked.RemoveDynamic(this, &UArcaneLobbyPanelWidget::HandleAddBot1Clicked);
	}
	if (Button_RemoveBot1)
	{
		Button_RemoveBot1->OnClicked.RemoveDynamic(this, &UArcaneLobbyPanelWidget::HandleRemoveBot1Clicked);
	}
	if (Button_BalanceBots)
	{
		Button_BalanceBots->OnClicked.RemoveDynamic(this, &UArcaneLobbyPanelWidget::HandleBalanceClicked);
	}
	if (Button_CapMinus)
	{
		Button_CapMinus->OnClicked.RemoveDynamic(this, &UArcaneLobbyPanelWidget::HandleCapMinusClicked);
	}
	if (Button_CapPlus)
	{
		Button_CapPlus->OnClicked.RemoveDynamic(this, &UArcaneLobbyPanelWidget::HandleCapPlusClicked);
	}
	if (Button_RoundsMinus)
	{
		Button_RoundsMinus->OnClicked.RemoveDynamic(this, &UArcaneLobbyPanelWidget::HandleRoundsMinusClicked);
	}
	if (Button_RoundsPlus)
	{
		Button_RoundsPlus->OnClicked.RemoveDynamic(this, &UArcaneLobbyPanelWidget::HandleRoundsPlusClicked);
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
	int32 Bots0 = 0;
	int32 Bots1 = 0;
	
	for (const APlayerState* PS : GS->PlayerArray)
	{
		if (const AArcanePlayerState* ArcanePS = Cast<AArcanePlayerState>(PS))
		{
			if (ArcanePS->TeamID == 0)
			{
				Count0++;
				if (ArcanePS->IsBot())
				{
					Bots0++;
				}
			}
			else if (ArcanePS->TeamID == 1)
			{
				Count1++;
				if (ArcanePS->IsBot())
				{
					Bots1++;
				}
			}
		}
	}

	if (Text_Count0)
	{
		const FString S = (Bots0 > 0)
			? FString::Printf(TEXT("蓝队  %d 人 · AI %d"), Count0, Bots0)
			: FString::Printf(TEXT("蓝队  %d 人"), Count0);
		Text_Count0->SetText(FText::FromString(S));
	}
	if (Text_Count1)
	{
		const FString S = (Bots1 > 0)
			? FString::Printf(TEXT("红队  %d 人 · AI %d"), Count1, Bots1)
			: FString::Printf(TEXT("红队  %d 人"), Count1);
		Text_Count1->SetText(FText::FromString(S));
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
			Text_Hint->SetText(NSLOCTEXT("ArcaneUI", "LobbyHost", "你是房主：可选阵营、为队伍分配 AI，再开始比赛"));
		}
		else
		{
			Text_Hint->SetText(NSLOCTEXT("ArcaneUI", "LobbyGuest", "已加入队伍，等待房主开始..."));
		}
	}
	
	// —— 房主 AI 控件：非房主整体隐藏；房主按满员/有无 AI 禁用 ——
	const bool bHost = (LocalPS != nullptr && LocalPS->IsRoomOwner());
	const int32 MaxPerTeam = FMath::Max(1, GS->MaxPlayersPerTeam);
	
	UButton* HostButtons[] = { Button_AddBot0, Button_RemoveBot0, Button_AddBot1, Button_RemoveBot1, Button_BalanceBots,
		Button_CapMinus, Button_CapPlus, Button_RoundsMinus, Button_RoundsPlus };
	
	for (UButton* Btn : HostButtons)
	{
		if (Btn != nullptr)
		{
			Btn->SetVisibility(bHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
	}
	
	// 整行折叠：非房主连"上限/赛制"这些房间规则都不该看到（房主特权信息）
	if (Box_Capacity)
	{
		Box_Capacity->SetVisibility(bHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (Text_Capacity)
	{
		Text_Capacity->SetText(FText::FromString(FString::Printf(TEXT("每队上限  %d"), MaxPerTeam)));
	}
	if (Box_Rounds)
	{
		Box_Rounds->SetVisibility(bHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (Text_Rounds)
	{
		Text_Rounds->SetText(FText::FromString(FString::Printf(TEXT("胜利条件  先赢 %d 局"), FMath::Max(1, GS->RoundsToWin))));
	}
	
	if (bHost)
	{
		if (Button_AddBot0)
		{
			Button_AddBot0->SetIsEnabled(Count0 < MaxPerTeam);
		}
		if (Button_RemoveBot0)
		{
			Button_RemoveBot0->SetIsEnabled(Bots0 > 0);
		}
		if (Button_AddBot1)
		{
			Button_AddBot1->SetIsEnabled(Count1 < MaxPerTeam);
		}
		if (Button_RemoveBot1)
		{
			Button_RemoveBot1->SetIsEnabled(Bots1 > 0);
		}
		// 两队全空无意义（服务端同规则拒绝）；其余幂等，多点几下没有副作用
		if (Button_BalanceBots)
		{
			Button_BalanceBots->SetIsEnabled(Count0 > 0 || Count1 > 0);
		}
		
		// 上下限：硬上限由服务端镜像下来，"+"到顶就置灰（服务端还会按出生点数再收窄一层）
		const int32 HardCap = FMath::Max(1, GS->MaxPlayersPerTeamHardCap);
		if (Button_CapMinus)
		{
			Button_CapMinus->SetIsEnabled(MaxPerTeam > 1);
		}
		if (Button_CapPlus)
		{
			Button_CapPlus->SetIsEnabled(MaxPerTeam < HardCap);
		}
		
		const int32 Rounds = FMath::Max(1, GS->RoundsToWin);
		const int32 RoundsCap = FMath::Max(1, GS->RoundsToWinMax);
		if (Button_RoundsMinus)
		{
			Button_RoundsMinus->SetIsEnabled(Rounds > 1);
		}
		if (Button_RoundsPlus)
		{
			Button_RoundsPlus->SetIsEnabled(Rounds < RoundsCap);
		}
	}
}

void UArcaneLobbyPanelWidget::HandleCapMinusClicked()
{
	const AArcaneGameState* GS = GetWorld()->GetGameState<AArcaneGameState>();
	if (GS == nullptr)
	{
		return;
	}
	
	SendMatchCommand(
		EArcaneMatchCommand::SetTeamCapacity,
		FMath::Max(1, GS->MaxPlayersPerTeam - 1));
}

void UArcaneLobbyPanelWidget::HandleCapPlusClicked()
{
	const AArcaneGameState* GS = GetWorld()->GetGameState<AArcaneGameState>();
	if (GS == nullptr)
	{
		return;
	}
	
	SendMatchCommand(
		EArcaneMatchCommand::SetTeamCapacity,
		GS->MaxPlayersPerTeam + 1);
}

void UArcaneLobbyPanelWidget::HandleRoundsMinusClicked()
{
	const AArcaneGameState* GS = GetWorld()->GetGameState<AArcaneGameState>();
	if (GS == nullptr)
	{
		return;
	}
	
	SendMatchCommand(
		EArcaneMatchCommand::SetRoundsToWin,
		FMath::Max(1, GS->RoundsToWin - 1));
}

void UArcaneLobbyPanelWidget::HandleRoundsPlusClicked()
{
	const AArcaneGameState* GS = GetWorld()->GetGameState<AArcaneGameState>();
	if (GS == nullptr)
	{
		return;
	}
	
	SendMatchCommand(
		EArcaneMatchCommand::SetRoundsToWin,
		GS->RoundsToWin + 1);
}

IArcaneMatchClient* UArcaneLobbyPanelWidget::GetMatchClient() const
{
	return Cast<IArcaneMatchClient>(GetOwningPlayer());
}

void UArcaneLobbyPanelWidget::SendMatchCommand(EArcaneMatchCommand Command, int32 Param)
{
	if (IArcaneMatchClient* Client = GetMatchClient())
	{
		Client->RequestMatchCommand(Command, Param);
	}
}

void UArcaneLobbyPanelWidget::HandleAddBot0Clicked()
{
	SendMatchCommand(EArcaneMatchCommand::AddBot, 0);
}

void UArcaneLobbyPanelWidget::HandleAddBot1Clicked()
{
	SendMatchCommand(EArcaneMatchCommand::AddBot, 1);
}

void UArcaneLobbyPanelWidget::HandleRemoveBot0Clicked()
{
	SendMatchCommand(EArcaneMatchCommand::RemoveBot, 0);
}

void UArcaneLobbyPanelWidget::HandleRemoveBot1Clicked()
{
	SendMatchCommand(EArcaneMatchCommand::RemoveBot, 1);
}

void UArcaneLobbyPanelWidget::HandleBalanceClicked()
{
	SendMatchCommand(EArcaneMatchCommand::BalanceBots);
}

void UArcaneLobbyPanelWidget::HandleTeam0Clicked()
{
	SendMatchCommand(EArcaneMatchCommand::ChangeTeam, 0);
}

void UArcaneLobbyPanelWidget::HandleTeam1Clicked()
{
	SendMatchCommand(EArcaneMatchCommand::ChangeTeam, 1);
}

void UArcaneLobbyPanelWidget::HandleStartClicked()
{
	SendMatchCommand(EArcaneMatchCommand::StartMatch);
}
