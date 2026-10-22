// Fill out your copyright notice in the Description page of Project Settings.

#include "ArcaneCombatHUDWidget.h"
#include "Framework/Text/TextLayout.h"

#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "GAS/ArcaneAbilitySystemComponent.h"
#include "GAS/ArcaneAttributeSet.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Player/ArcanePlayerController.h"
#include "Player/ArcanePlayerState.h"
#include "UI/ArcaneLobbyPanelWidget.h"
#include "UI/ArcaneMatchResultPanelWidget.h"
#include "UI/ArcaneUITheme.h"

void UArcaneCombatHUDWidget::NativeOnConstructed()
{
	// 技能槽装配：绑定存在才纳入每帧冷却刷新
	auto AddSlot = [&](EArcaneAbilityInputID InputID, UBorder* Dim, UImage* Icon, UTextBlock* Text)
	{
		if (Dim || Icon || Text)
		{
			FAbilitySlot NewSlot;
			NewSlot.InputID = InputID;
			NewSlot.Dim = Dim;
			NewSlot.Icon = Icon;
			NewSlot.CDText = Text;
			Slots.Add(NewSlot);
		}
	};
	AddSlot(EArcaneAbilityInputID::Dash, Border_Dash, Icon_Dash, Text_CD_Dash);
	AddSlot(EArcaneAbilityInputID::SecondaryAbility, Border_Secondary, Icon_Secondary, Text_CD_Secondary);
	AddSlot(EArcaneAbilityInputID::Ultimate, Border_Ultimate, Icon_Ultimate, Text_CD_Ultimate);

	// 能量：绑定本机 PlayerState 的 ASC。加入客户端 HUD 构造时 PlayerState 往往
	// 还没复制到 —— 这里失败不致命，NativeTick 里 TryBindLocalASC 会补绑。
	TryBindLocalASC();

	// 观战提示条：绑本地 PC 的目标切换委托；初始收起（没死过就永远不该亮）
	TryBindSpectateDelegate();
	if (Text_Spectate)
	{
		Text_Spectate->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (AArcaneGameState* GS = GetWorld()->GetGameState<AArcaneGameState>())
	{
		GS->OnScoreChanged.AddDynamic(this, &UArcaneCombatHUDWidget::HandleScoreChanged);
		GS->OnRoundNumberChanged.AddDynamic(this, &UArcaneCombatHUDWidget::HandleRoundChanged);
		GS->OnPhaseChanged.AddDynamic(this, &UArcaneCombatHUDWidget::HandlePhaseChanged);
	}

	RefreshScores();
	RefreshPhaseDisplay();
	RefreshAliveCounts();

	// —— 主题：比分条/技能栏卡片底 + 能量条药丸化；冷却遮罩套圆角刷（着色仍由
	// UpdateAbilitySlot 的 SetBrushColor 负责，画刷 Tint 保持纯白做乘法基底）——
	if (Border_ScoreBar)
	{
		ArcaneUIStyle::ApplyPanel(Border_ScoreBar, FLinearColor(0.045f, 0.055f, 0.085f, 0.82f),
			ArcaneUITheme::RadiusCard, 1.f, ArcaneUITheme::OutlineSoft);
		Border_ScoreBar->SetPadding(FMargin(20.f, 6.f));
	}
	if (Border_AbilityBar)
	{
		ArcaneUIStyle::ApplyPanel(Border_AbilityBar, FLinearColor(0.045f, 0.055f, 0.085f, 0.60f),
			ArcaneUITheme::RadiusCard);
		Border_AbilityBar->SetPadding(FMargin(12.f, 10.f));
	}
	if (EnergyBar)
	{
		ArcaneUIStyle::ApplyProgressBar(EnergyBar);
	}
	UBorder* Dims[] = { Border_Dash, Border_Secondary, Border_Ultimate };
	for (UBorder* Dim : Dims)
	{
		if (Dim)
		{
			// 画刷 Tint=白做乘法基底，颜色/透明度完全交给 BrushColor。
			// 初始必须置全透明：加入客户端 ASC 复制晚，若遮罩带着不透明白等
			// 首个 tick，技能槽会整排显示成白方块（且 ASC 迟迟不到就永不恢复）。
			ArcaneUIStyle::ApplyPanel(Dim, FLinearColor::White, ArcaneUITheme::RadiusChip, 0.f, FLinearColor::Transparent);
			Dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
		}
	}
}

bool UArcaneCombatHUDWidget::TryBindLocalASC()
{
	if (BoundASC != nullptr)
	{
		return true;
	}
	AArcanePlayerState* PS = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<AArcanePlayerState>() : nullptr;
	UArcaneAbilitySystemComponent* ArcaneASC = PS ? PS->GetArcaneASC() : nullptr;
	if (ArcaneASC == nullptr)
	{
		return false;
	}

	InitFromASC(ArcaneASC);
	BindAttribute(UArcaneAttributeSet::GetEnergyAttribute());
	BindAttribute(UArcaneAttributeSet::GetMaxEnergyAttribute());
	ArcaneASC->OnAbilityCommitted.AddDynamic(this, &UArcaneCombatHUDWidget::HandleAbilityCommitted);

	// 属性变化只在增量时广播，绑定瞬间要主动同步一次现值
	const float Cur = ArcaneASC->GetNumericAttribute(UArcaneAttributeSet::GetEnergyAttribute());
	const float Max = FMath::Max(1.0f, ArcaneASC->GetNumericAttribute(UArcaneAttributeSet::GetMaxEnergyAttribute()));
	MaxEnergySeen = Max;
	TargetEnergy = FMath::Clamp(Cur / Max, 0.0f, 1.0f);
	CurrentEnergy = TargetEnergy;
	return true;
}

bool UArcaneCombatHUDWidget::TryBindSpectateDelegate()
{
	if (BoundSpectatePC.IsValid())
	{
		return true;
	}

	AArcanePlayerController* PC = Cast<AArcanePlayerController>(GetOwningPlayer());
	if (PC == nullptr)
	{
		return false;
	}

	BoundSpectatePC = PC;
	PC->OnSpectateTargetChanged.AddDynamic(this, &UArcaneCombatHUDWidget::HandleSpectateTargetChanged);
	return true;
}

void UArcaneCombatHUDWidget::NativeDestruct()
{
	if (AArcaneGameState* GS = GetWorld()->GetGameState<AArcaneGameState>())
	{
		GS->OnScoreChanged.RemoveDynamic(this, &UArcaneCombatHUDWidget::HandleScoreChanged);
		GS->OnRoundNumberChanged.RemoveDynamic(this, &UArcaneCombatHUDWidget::HandleRoundChanged);
		GS->OnPhaseChanged.RemoveDynamic(this, &UArcaneCombatHUDWidget::HandlePhaseChanged);
	}

	if (UArcaneAbilitySystemComponent* ArcaneASC = GetArcaneASC())
	{
		ArcaneASC->OnAbilityCommitted.RemoveDynamic(this, &UArcaneCombatHUDWidget::HandleAbilityCommitted);
	}

	if (AArcanePlayerController* PC = BoundSpectatePC.Get())
	{
		PC->OnSpectateTargetChanged.RemoveDynamic(this, &UArcaneCombatHUDWidget::HandleSpectateTargetChanged);
	}
	BoundSpectatePC = nullptr;

	Slots.Empty();

	Super::NativeDestruct();
}

UArcaneAbilitySystemComponent* UArcaneCombatHUDWidget::GetArcaneASC() const
{
	return Cast<UArcaneAbilitySystemComponent>(BoundASC);
}

void UArcaneCombatHUDWidget::HandleScoreChanged()
{
	RefreshScores();
}

void UArcaneCombatHUDWidget::HandleRoundChanged()
{
	RefreshScores();
}

void UArcaneCombatHUDWidget::HandlePhaseChanged(EArcaneMatchPhase NewPhase)
{
	RefreshPhaseDisplay();
}

void UArcaneCombatHUDWidget::HandleAbilityCommitted()
{
	// 施法瞬间能量被 Cost 扣减，等属性回调即可；这里只重刷一次可用性底色
	for (FAbilitySlot& Entry : Slots)
	{
		UpdateAbilitySlot(Entry, 0.0f);
	}
}

void UArcaneCombatHUDWidget::HandleSpectateTargetChanged(AActor* NewTarget)
{
	if (Text_Spectate == nullptr)
	{
		return;
	}

	// LeaveSpectatorMode 广播 nullptr → 收起
	if (NewTarget == nullptr)
	{
		Text_Spectate->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// Bot 与人机同规则：Pawn → PlayerState → 名字（Phase 9 身份统一后这条路径通用）
	FString TargetName = TEXT("???");
	if (const APawn* TargetPawn = Cast<APawn>(NewTarget))
	{
		if (const APlayerState* PS = TargetPawn->GetPlayerState<APlayerState>())
		{
			TargetName = PS->GetPlayerName();
		}
	}

	Text_Spectate->SetText(FText::FromString(FString::Printf(
		TEXT("正在观战 %s    E / 滚轮下 下一位 · Q / 滚轮上 上一位"), *TargetName)));
	Text_Spectate->SetVisibility(ESlateVisibility::HitTestInvisible);
	ArcaneUIStyle::ApplyText(Text_Spectate, ArcaneUITheme::FontBody, ArcaneUITheme::TextSecondary);
	Text_Spectate->SetJustification(ETextJustify::Center);
}

void UArcaneCombatHUDWidget::RefreshScores()
{
	const AArcaneGameState* GS = GetWorld()->GetGameState<AArcaneGameState>();
	if (GS == nullptr)
	{
		return;
	}

	const int32 Score0 = GS->TeamScores.IsValidIndex(0) ? GS->TeamScores[0] : 0;
	const int32 Score1 = GS->TeamScores.IsValidIndex(1) ? GS->TeamScores[1] : 0;

	if (Text_Team0Score)
	{
		Text_Team0Score->SetText(FText::AsNumber(Score0));
		ArcaneUIStyle::ApplyText(Text_Team0Score, ArcaneUITheme::FontH2, ArcaneUITheme::TeamBlue);
	}
	if (Text_Team1Score)
	{
		Text_Team1Score->SetText(FText::AsNumber(Score1));
		ArcaneUIStyle::ApplyText(Text_Team1Score, ArcaneUITheme::FontH2, ArcaneUITheme::TeamRed);
	}
	if (Text_Round)
	{
		Text_Round->SetText(FText::FromString(FString::Printf(TEXT("第 %d 回合 · 先赢 %d 局"), GS->RoundNumber, FMath::Max(1, GS->RoundsToWin))));
		ArcaneUIStyle::ApplyText(Text_Round, ArcaneUITheme::FontBody, ArcaneUITheme::TextSecondary);
	}
}

void UArcaneCombatHUDWidget::RefreshAliveCounts()
{
	const AArcaneGameState* GS = GetWorld()->GetGameState<AArcaneGameState>();
	if (GS == nullptr)
	{
		return;
	}

	// 存活数就地从复制下来的 PlayerState 现算（TeamID + bIsDead 都已经是权威镜像），
	// 不再往 GameState 塞一份派生副本 —— 两处真相迟早对不上。
	// Bot 计入：它和真人一样占场地、一样被团灭判定数（AArcaneGameMode::IsTeamWiped 同一套人）。
	// 比分条固定左右两栏，两队是全局前提 → 写死 2，不做通用化。
	int32 Alive[2] = { 0, 0 };
	for (const APlayerState* PS : GS->PlayerArray)
	{
		const AArcanePlayerState* ArcPS = Cast<AArcanePlayerState>(PS);
		if (ArcPS == nullptr || ArcPS->bIsDead || ArcPS->TeamID < 0 || ArcPS->TeamID >= 2)
		{
			continue;
		}
		Alive[ArcPS->TeamID]++;
	}

	UTextBlock* Texts[2] = { Text_Team0Alive, Text_Team1Alive };
	const FLinearColor Colors[2] = { ArcaneUITheme::TeamBlue, ArcaneUITheme::TeamRed };
	for (int32 Team = 0; Team < 2; Team++)
	{
		if (Texts[Team] == nullptr || CachedTeamAlive[Team] == Alive[Team])
		{
			continue;
		}

		CachedTeamAlive[Team] = Alive[Team];
		Texts[Team]->SetText(FText::FromString(FString::Printf(TEXT("存活 %d"), Alive[Team])));
		ArcaneUIStyle::ApplyText(Texts[Team], ArcaneUITheme::FontBody, Colors[Team]);
		Texts[Team]->SetJustification(ETextJustify::Center);
	}
}

void UArcaneCombatHUDWidget::RefreshPhaseDisplay()
{
	const AArcaneGameState* GS = GetWorld()->GetGameState<AArcaneGameState>();
	const EArcaneMatchPhase Phase = GS ? GS->MatchPhase : EArcaneMatchPhase::Warmup;

	const bool bWarmup = Phase == EArcaneMatchPhase::Warmup;
	const bool bMatchEnd = Phase == EArcaneMatchPhase::MatchEnd;

	if (Group_ScoreBar)
	{
		const ESlateVisibility BarVis = bWarmup ? ESlateVisibility::Collapsed : ESlateVisibility::Visible;
		Group_ScoreBar->SetVisibility(BarVis);
		// 美化包裹层：卡片在 Group 外层，只收内层会留一张空卡片，需同步显隐
		if (Border_ScoreBar) { Border_ScoreBar->SetVisibility(BarVis); }
	}
	if (Group_AbilityBar)
	{
		const ESlateVisibility BarVis = bWarmup ? ESlateVisibility::Collapsed : ESlateVisibility::Visible;
		Group_AbilityBar->SetVisibility(BarVis);
		if (Border_AbilityBar) { Border_AbilityBar->SetVisibility(BarVis); }
	}
	if (Panel_Lobby)
	{
		Panel_Lobby->SetVisibility(bWarmup ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (Panel_Result)
	{
		if (bMatchEnd)
		{
			Panel_Result->RefreshResult();
		}
		Panel_Result->SetVisibility(bMatchEnd ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (Text_Phase)
	{
		// 与旧蓝图行为一致：只在 RoundEnd 亮阶段提示，其余时候收起
		const bool bShowPhaseText = (Phase == EArcaneMatchPhase::RoundEnd);
		Text_Phase->SetVisibility(bShowPhaseText ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		Text_Phase->SetText(NSLOCTEXT("ArcaneUI", "PhaseRoundEnd", "回合结算"));
		ArcaneUIStyle::ApplyText(Text_Phase, ArcaneUITheme::FontH1, ArcaneUITheme::TextGlow);
		Text_Phase->SetJustification(ETextJustify::Center);
	}
}

void UArcaneCombatHUDWidget::NativeOnAttributeChanged(const FGameplayAttribute& Attribute, float NewValue, float OldValue)
{
	if (Attribute == UArcaneAttributeSet::GetEnergyAttribute())
	{
		TargetEnergy = FMath::Clamp(NewValue / FMath::Max(1.0f, MaxEnergySeen), 0.0f, 1.0f);
	}
	else if (Attribute == UArcaneAttributeSet::GetMaxEnergyAttribute())
	{
		MaxEnergySeen = FMath::Max(1.0f, NewValue);
		if (BoundASC)
		{
			const float Energy = BoundASC->GetNumericAttribute(UArcaneAttributeSet::GetEnergyAttribute());
			TargetEnergy = FMath::Clamp(Energy / MaxEnergySeen, 0.0f, 1.0f);
		}
	}
}

void UArcaneCombatHUDWidget::UpdateAbilitySlot(FAbilitySlot& Entry, float DeltaTime)
{
	const UArcaneAbilitySystemComponent* ArcaneASC = GetArcaneASC();
	float Remaining = 0.0f;
	float Duration = 0.0f;
	// ASC 未绑定（加入客户端 PlayerState 复制晚）按"就绪"渲染：
	// 遮罩透明、CD 隐藏。绝不能提前 return —— 那会把构造期的遮罩状态永久冻在槽上
	const bool bOnCooldown = ArcaneASC != nullptr
		&& ArcaneASC->GetCooldownTimes(Entry.InputID, Remaining, Duration);

	float DimAlpha = 0.0f;
	if (bOnCooldown)
	{
		DimAlpha = FMath::Clamp(Remaining / FMath::Max(0.01f, Duration), 0.0f, 1.0f) * ArcaneUITheme::SlotCooling.A;
		if (Entry.CDText)
		{
			Entry.CDText->SetVisibility(ESlateVisibility::HitTestInvisible);
			Entry.CDText->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), Remaining)));
		}
	}
	else
	{
		if (Entry.CDText)
		{
			Entry.CDText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (Entry.Dim)
	{
		FLinearColor Color = ArcaneUITheme::SlotCooling;
		Color.A = DimAlpha;
		Entry.Dim->SetBrushColor(Color);
	}
	if (Entry.Icon)
	{
		Entry.Icon->SetColorAndOpacity(bOnCooldown ? FLinearColor(0.4f, 0.4f, 0.45f) : ArcaneUITheme::SlotIdle);
	}
}

void UArcaneCombatHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 加入客户端：PlayerState/ASC 复制晚于 HUD 构造，惰性补绑一次即止
	if (BoundASC == nullptr)
	{
		TryBindLocalASC();
	}

	// 同理兜底 PC 晚于 HUD 构造的罕见时序
	if (!BoundSpectatePC.IsValid())
	{
		TryBindSpectateDelegate();
	}

	// 存活数没有专门的复制通知（bIsDead 只复制、不广播），现算是这里唯一的刷新口
	RefreshAliveCounts();

	if (EnergyBar)
	{
		CurrentEnergy = FMath::FInterpTo(CurrentEnergy, TargetEnergy, InDeltaTime, ArcaneUITheme::EnergyInterpSpeed);
		EnergyBar->SetPercent(CurrentEnergy);
		EnergyBar->SetFillColorAndOpacity(CurrentEnergy < 0.15f
			? FLinearColor(0.55f, 0.60f, 0.85f)   // 能量枯竭：偏灰蓝
			: ArcaneUITheme::Energy);
	}

	for (FAbilitySlot& Entry : Slots)
	{
		UpdateAbilitySlot(Entry, InDeltaTime);
	}
}
