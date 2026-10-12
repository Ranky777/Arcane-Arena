// Fill out your copyright notice in the Description page of Project Settings.

#include "ArcaneMainMenuWidget.h"
#include "Framework/Text/TextLayout.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"

#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Online/ArcaneSessionSubsystem.h"
#include "ArcaneSessionRowWidget.h"
#include "ArcaneSettingsWidget.h"
#include "ArcaneUITheme.h"

UArcaneSessionSubsystem* UArcaneMainMenuWidget::GetSessionSubsystem() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UArcaneSessionSubsystem>() : nullptr;
}

void UArcaneMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// —— 主题：深色底衬 + 大标题 + 统一按钮体系 ——
	if (Border_Backdrop)
	{
		Border_Backdrop->SetBrushColor(ArcaneUITheme::BgDeep);
	}
	if (Border_Card)
	{
		ArcaneUIStyle::ApplyPanel(Border_Card, ArcaneUITheme::CardBg, ArcaneUITheme::RadiusPanel,
			1.f, ArcaneUITheme::OutlineSoft);
		Border_Card->SetPadding(FMargin(30.f, 26.f));
	}
	if (Text_Title)
	{
		Text_Title->SetText(NSLOCTEXT("ArcaneMainMenu", "Title", "ARCANE ARENA"));
		ArcaneUIStyle::ApplyTitle(Text_Title);
		Text_Title->SetJustification(ETextJustify::Center);
	}
	ArcaneUIStyle::ApplyRowSpacing(Panel_Main, ArcaneUITheme::PadRow);
	ArcaneUIStyle::ApplyButton(Button_Create, EArcaneButtonTone::Primary);
	ArcaneUIStyle::ApplyButton(Button_Refresh, EArcaneButtonTone::Secondary);
	ArcaneUIStyle::ApplyButton(Button_Join, EArcaneButtonTone::Success);
	ArcaneUIStyle::ApplyButton(Button_Settings, EArcaneButtonTone::Secondary);
	if (Text_Status)
	{
		ArcaneUIStyle::ApplyText(Text_Status, ArcaneUITheme::FontSmall, ArcaneUITheme::TextSecondary);
		Text_Status->SetJustification(ETextJustify::Center);
	}

	if (UArcaneSessionSubsystem* Sub = GetSessionSubsystem())
	{
		Sub->OnArcaneSessionFound.AddDynamic(this, &UArcaneMainMenuWidget::HandleSessionsFound);
	}

	if (Button_Create)
	{
		Button_Create->OnClicked.AddDynamic(this, &UArcaneMainMenuWidget::HandleCreateClicked);
	}
	if (Button_Refresh)
	{
		Button_Refresh->OnClicked.AddDynamic(this, &UArcaneMainMenuWidget::HandleRefreshClicked);
	}
	if (Button_Join)
	{
		Button_Join->OnClicked.AddDynamic(this, &UArcaneMainMenuWidget::HandleJoinClicked);
	}
	if (Button_Settings)
	{
		Button_Settings->OnClicked.AddDynamic(this, &UArcaneMainMenuWidget::HandleSettingsClicked);
	}

	SetStatus(TEXT("创建房间开始游戏，或刷新加入他人房间"));
}

void UArcaneMainMenuWidget::NativeDestruct()
{
	if (UArcaneSessionSubsystem* Sub = GetSessionSubsystem())
	{
		Sub->OnArcaneSessionFound.RemoveDynamic(this, &UArcaneMainMenuWidget::HandleSessionsFound);
	}

	if (Button_Create)
	{
		Button_Create->OnClicked.RemoveDynamic(this, &UArcaneMainMenuWidget::HandleCreateClicked);
	}
	if (Button_Refresh)
	{
		Button_Refresh->OnClicked.RemoveDynamic(this, &UArcaneMainMenuWidget::HandleRefreshClicked);
	}
	if (Button_Join)
	{
		Button_Join->OnClicked.RemoveDynamic(this, &UArcaneMainMenuWidget::HandleJoinClicked);
	}
	if (Button_Settings)
	{
		Button_Settings->OnClicked.RemoveDynamic(this, &UArcaneMainMenuWidget::HandleSettingsClicked);
	}

	Super::NativeDestruct();
}

void UArcaneMainMenuWidget::HandleSessionsFound(const TArray<FArcaneSessionInfo>& Sessions)
{
	RebuildSessionList(Sessions);
	SetStatus(Sessions.Num() > 0
		? FString::Printf(TEXT("发现 %d 个房间，点击行加入"), Sessions.Num())
		: FString(TEXT("没有发现房间，可创建一个")));
}

void UArcaneMainMenuWidget::RebuildSessionList(const TArray<FArcaneSessionInfo>& Sessions)
{
	if (ScrollBox_Sessions == nullptr)
	{
		return;
	}

	ScrollBox_Sessions->ClearChildren();
	SpawnedRows.Empty();
	SelectedSearchIndex = INDEX_NONE;
	
	if (SessionRowClass == nullptr)
	{
		SetStatus(TEXT("SessionRowClass 未配置（WBP 迁移：Defaults 里指认 WBP_SessionRow 的新基类子类）"));
		return;
	}

	for (const FArcaneSessionInfo& Info : Sessions)
	{
		UArcaneSessionRowWidget* Row = CreateWidget<UArcaneSessionRowWidget>(GetOwningPlayer(), SessionRowClass);
		if (Row == nullptr)
		{
			continue;
		}
		Row->SetupFromInfo(Info);
		Row->OnRowClicked.AddDynamic(this, &UArcaneMainMenuWidget::HandleRowClicked);
		ScrollBox_Sessions->AddChild(Row);
		SpawnedRows.Add(Row);
	}
}

void UArcaneMainMenuWidget::HandleRowClicked(const FArcaneSessionInfo& Info, int32 SearchIndex)
{
	// 行按发现顺序加入列表，行下标 == SearchIndex
	for (UArcaneSessionRowWidget* Row : SpawnedRows)
	{
		Row->SetSelected(false);
	}
	if (SpawnedRows.IsValidIndex(SearchIndex))
	{
		SpawnedRows[SearchIndex]->SetSelected(true);
	}
	SelectedSearchIndex = SearchIndex;

	if (UArcaneSessionSubsystem* Sub = GetSessionSubsystem())
	{
		Sub->JoinArcaneSession(SearchIndex);
		SetStatus(FString::Printf(TEXT("正在加入 %s ..."), *Info.SessionName));
	}
}

void UArcaneMainMenuWidget::HandleCreateClicked()
{
	if (UArcaneSessionSubsystem* Sub = GetSessionSubsystem())
	{
		Sub->CreateArcaneSession();
		SetStatus(TEXT("正在创建房间..."));
	}
}

void UArcaneMainMenuWidget::HandleRefreshClicked()
{
	if (UArcaneSessionSubsystem* Sub = GetSessionSubsystem())
	{
		Sub->FindArcaneSessions();
		SetStatus(TEXT("正在搜索房间..."));
	}
}

void UArcaneMainMenuWidget::HandleJoinClicked()
{
	if (SelectedSearchIndex == INDEX_NONE)
	{
		SetStatus(TEXT("请先在列表点击一个房间"));
		return;
	}
	if (UArcaneSessionSubsystem* Sub = GetSessionSubsystem())
	{
		Sub->JoinArcaneSession(SelectedSearchIndex);
	}
}

void UArcaneMainMenuWidget::HandleSettingsClicked()
{
	if (SettingsWidgetClass == nullptr)
	{
		SetStatus(TEXT("设置面板未配置（WBP_MainMenu.Defaults 指认 SettingsWidgetClass）"));
		return;
	}

	if (SettingsPanel == nullptr)
	{
		SettingsPanel = CreateWidget<UArcaneSettingsWidget>(GetOwningPlayer(), SettingsWidgetClass);
		if (SettingsPanel)
		{
			SettingsPanel->AddToViewport(50);
		}
	}
	else
	{
		SettingsPanel->RemoveFromParent();
		SettingsPanel->AddToViewport(50);
	}

	if (SettingsPanel)
	{
		SettingsPanel->RefreshFromUserSettings();
	}
}

void UArcaneMainMenuWidget::SetStatus(const FString& Text)
{
	if (Text_Status)
	{
		Text_Status->SetText(FText::FromString(Text));
		Text_Status->SetColorAndOpacity(ArcaneUITheme::TextSecondary);
	}
}
