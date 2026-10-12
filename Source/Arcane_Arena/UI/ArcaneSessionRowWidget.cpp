// Fill out your copyright notice in the Description page of Project Settings.

#include "ArcaneSessionRowWidget.h"
#include "Framework/Text/TextLayout.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "ArcaneUITheme.h"

void UArcaneSessionRowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_Row)
	{
		Button_Row->OnClicked.AddDynamic(this, &UArcaneSessionRowWidget::HandleRowClicked);
		ArcaneUIStyle::ApplyRowButton(Button_Row);
	}
	if (Text_RoomName)
	{
		ArcaneUIStyle::ApplyText(Text_RoomName, ArcaneUITheme::FontBody, ArcaneUITheme::TextPrimary);
	}
	if (Text_Players)
	{
		ArcaneUIStyle::ApplyText(Text_Players, ArcaneUITheme::FontBody, ArcaneUITheme::TextSecondary);
		Text_Players->SetJustification(ETextJustify::Right);
	}
}

void UArcaneSessionRowWidget::NativeDestruct()
{
	if (Button_Row)
	{
		Button_Row->OnClicked.RemoveDynamic(this, &UArcaneSessionRowWidget::HandleRowClicked);
	}

	Super::NativeDestruct();
}

void UArcaneSessionRowWidget::SetupFromInfo(const FArcaneSessionInfo& InInfo)
{
	CachedInfo = InInfo;

	if (Text_RoomName)
	{
		FString Label = InInfo.SessionName;
		if (InInfo.Ping >= 0)
		{
			Label += FString::Printf(TEXT("   %d ms"), InInfo.Ping);
		}
		Text_RoomName->SetText(FText::FromString(Label));
	}
	if (Text_Players)
	{
		Text_Players->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), InInfo.CurrentPlayers, InInfo.MaxPlayers)));
		Text_Players->SetColorAndOpacity(InInfo.CurrentPlayers < InInfo.MaxPlayers ? ArcaneUITheme::TextPrimary : ArcaneUITheme::HealthEnemy);
	}
}

void UArcaneSessionRowWidget::SetSelected(bool bNewSelected)
{
	if (Button_Row)
	{
		// 选中 = 战队蓝圆角实底；未选 = 透明行刷（hover 才亮）
		if (bNewSelected)
		{
			ArcaneUIStyle::ApplyButton(Button_Row, ArcaneUITheme::TeamBlue, ArcaneUITheme::FontBody, ArcaneUITheme::RadiusChip);
		}
		else
		{
			ArcaneUIStyle::ApplyRowButton(Button_Row);
		}
	}
	bSelected = bNewSelected;
}

void UArcaneSessionRowWidget::HandleRowClicked()
{
	OnRowClicked.Broadcast(CachedInfo, CachedInfo.SearchIndex);
}
