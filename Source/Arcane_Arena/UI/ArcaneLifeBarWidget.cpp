// Fill out your copyright notice in the Description page of Project Settings.

#include "ArcaneLifeBarWidget.h"

#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GAS/ArcaneAttributeSet.h"
#include "UI/ArcaneUITheme.h"

void UArcaneLifeBarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	TargetFill = CurrentFill = 1.0f;

	if (Bar)
	{
		// 药丸血条：底槽深色圆角，填充色由关系/脉冲逻辑动态设置
		ArcaneUIStyle::ApplyProgressBar(Bar);
	}
	if (Text_Health)
	{
		ArcaneUIStyle::ApplyText(Text_Health, ArcaneUITheme::FontSmall, ArcaneUITheme::TextPrimary);
		Text_Health->SetJustification(ETextJustify::Center);
	}
	
	TargetShield = CurrentShield = 0.0f;
	CachedShieldRadius = -1.0f;
	bShieldRoundedLeft = false;
	
	if (ShieldSegment != nullptr)
	{
		ShieldSegment->SetColorAndOpacity(FLinearColor::White);
		ShieldSegment->SetVisibility(ESlateVisibility::Collapsed);
		
		if (UCanvasPanelSlot* PanelSlot = Cast<UCanvasPanelSlot>(ShieldSegment->Slot))
		{
			PanelSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
			PanelSlot->SetAlignment(FVector2D(0.f, 0.f));
			PanelSlot->SetAutoSize(false);
			PanelSlot->SetZOrder(1); // 盖在 Bar 之上
		}
	}
}

void UArcaneLifeBarWidget::NativeOnAttributeChanged(const FGameplayAttribute& Attribute, float NewValue, float OldValue)
{
	if (Attribute == UArcaneAttributeSet::GetHealthAttribute())
	{
		TargetFill = FMath::Clamp(NewValue / FMath::Max(1.0f, MaxHealthSeen), 0.0f, 1.0f);
		if (Text_Health)
		{
			Text_Health->SetText(FText::AsNumber(FMath::CeilToInt(NewValue)));
		}
	}
	else if (Attribute == UArcaneAttributeSet::GetMaxHealthAttribute())
	{
		MaxHealthSeen = FMath::Max(1.0f, NewValue);
		if (BoundASC)
		{
			const float Health = BoundASC->GetNumericAttribute(UArcaneAttributeSet::GetHealthAttribute());
			TargetFill = FMath::Clamp(Health / MaxHealthSeen, 0.0f, 1.0f);
		}
	}
	else if (Attribute == UArcaneAttributeSet::GetShieldAttribute())
	{
		TargetShield = FMath::Max(0.0f, NewValue);
	}
}

void UArcaneLifeBarWidget::NativeOnOwnerRelationSet(EArcaneOwnerRelation Relation)
{
	// 颜色只在这里统一决定；蓝图若还想加个性化表现可继续实现 OnOwnerRelationSet
	if (Bar)
	{
		const FLinearColor Color = Relation == EArcaneOwnerRelation::Self ? ArcaneUITheme::HealthSelf
			: Relation == EArcaneOwnerRelation::Ally ? ArcaneUITheme::HealthAlly
			: ArcaneUITheme::HealthEnemy;
		Bar->SetFillColorAndOpacity(Color);
	}
}

void UArcaneLifeBarWidget::UpdateShieldGeometry()
{
	if (Bar == nullptr || ShieldSegment == nullptr)
	{
		return;
	}
	
	const FVector2D BarSize = Bar->GetCachedGeometry().GetLocalSize();
	if (BarSize.X <= 0.0f || BarSize.Y <= 0.0f)
	{
		return;
	}
	
	const float MaxHealth = FMath::Max(1.0f, MaxHealthSeen);
	const float HealthPct = FMath::Clamp(CurrentFill, 0.0f, 1.0f);
	const float ShieldW = FMath::Clamp(CurrentShield / MaxHealth, 0.0f, 1.0f);
	
	if (ShieldW <= KINDA_SMALL_NUMBER)
	{
		ShieldSegment->SetVisibility(ESlateVisibility::Collapsed);
		
		return;
	}
	
	// 有空间：接在血条右缘；空间不够：被挤到右端，盖住一部分血条
	const float ShieldStart = FMath::Clamp(FMath::Min(HealthPct, 1.0f - ShieldW), 0.0f, 1.0f);
	
	const float Radius = BarSize.Y * 0.5f;
	const float StartX = ShieldStart * BarSize.X;
	
	// 护盾接在血条右缘时，向左补半个圆角，盖住血条填充的圆角缺口；
	// 被挤进血条内部时不需要补，左缘就是一道干净的分界线。
	const bool bAtHealthEdge = ShieldStart >= HealthPct - KINDA_SMALL_NUMBER;
	const float Extend = (bAtHealthEdge && ShieldStart > KINDA_SMALL_NUMBER)
		? FMath::Min(Radius, ShieldW * BarSize.X)
		: 0.0f;
	const float DrawStartX = FMath::Max(0.0f, StartX - Extend);
	const float DrawW = ShieldW * BarSize.X + (StartX - DrawStartX);
	
	if (UCanvasPanelSlot* PanelSlot = Cast<UCanvasPanelSlot>(ShieldSegment->Slot))
	{
		PanelSlot->SetPosition(FVector2D(DrawStartX, 0.0f));
		PanelSlot->SetSize(FVector2D(DrawW, BarSize.Y));
	}
	
	ShieldSegment->SetVisibility(ESlateVisibility::HitTestInvisible);
 
	// 只在“圆角半径变化”或“护盾到了最左端”时重建画刷，避免每帧 SetBrush
	const bool bRoundLeft = ShieldStart <= KINDA_SMALL_NUMBER;
	if (!FMath::IsNearlyEqual(Radius, CachedShieldRadius) || bRoundLeft != bShieldRoundedLeft)
	{
		CachedShieldRadius = Radius;
		bShieldRoundedLeft = bRoundLeft;
 
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(ArcaneUITheme::ShieldFill);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		// X=左上 Y=右上 Z=右下 W=左下：默认左平右圆
		Brush.OutlineSettings.CornerRadii = FVector4(
			bRoundLeft ? Radius : 0.0f,
			Radius,
			Radius,
			bRoundLeft ? Radius : 0.0f);
		Brush.ImageSize = FVector2D(16.0f, 16.0f);
		ShieldSegment->SetBrush(Brush);
	}
}

void UArcaneLifeBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (Bar == nullptr)
	{
		return;
	}

	CurrentFill = FMath::FInterpTo(CurrentFill, TargetFill, InDeltaTime, ArcaneUITheme::BarInterpSpeed);
	Bar->SetPercent(CurrentFill);
	
	CurrentShield = FMath::FInterpTo(CurrentShield, TargetShield, InDeltaTime, ArcaneUITheme::BarInterpSpeed);
	UpdateShieldGeometry();
}
