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

	// 能量：直接从本机 PlayerState 的 ASC 绑定，不再依赖蓝图 Construct 连线
	if (AArcanePlayerState* PS = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<AArcanePlayerState>() : nullptr)
	{
		if (PS->GetArcaneASC())
		{
			InitFromASC(PS->GetArcaneASC());
			BindAttribute(UArcaneAttributeSet::GetEnergyAttribute());
			BindAttribute(UArcaneAttributeSet::GetMaxEnergyAttribute());
		}
	}

	if (AArcaneGameState* GS = GetWorld()->GetGameState<AArcaneGameState>())
	{
		GS->OnScoreChanged.AddDynamic(this, &UArcaneCombatHUDWidget::HandleScoreChanged);
		GS->OnRoundNumberChanged.AddDynamic(this, &UArcaneCombatHUDWidget::HandleRoundChanged);
		GS->OnPhaseChanged.AddDynamic(this, &UArcaneCombatHUDWidget::HandlePhaseChanged);
	}

	if (UArcaneAbilitySystemComponent* ArcaneASC = GetArcaneASC())
	{
		ArcaneASC->OnAbilityCommitted.AddDynamic(this, &UArcaneCombatHUDWidget::HandleAbilityCommitted);
	}

	RefreshScores();
	RefreshPhaseDisplay();

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
			ArcaneUIStyle::ApplyPanel(Dim, FLinearColor::White, ArcaneUITheme::RadiusChip, 0.f, FLinearColor::Transparent);
		}
	}
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
		Text_Round->SetText(FText::FromString(FString::Printf(TEXT("第 %d 回合"), GS->RoundNumber)));
		ArcaneUIStyle::ApplyText(Text_Round, ArcaneUITheme::FontBody, ArcaneUITheme::TextSecondary);
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
		Group_ScoreBar->SetVisibility(bWarmup ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (Group_AbilityBar)
	{
		Group_AbilityBar->SetVisibility(bWarmup ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
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
	if (ArcaneASC == nullptr)
	{
		return;
	}

	float Remaining = 0.0f;
	float Duration = 0.0f;
	const bool bOnCooldown = ArcaneASC->GetCooldownTimes(Entry.InputID, Remaining, Duration);

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
