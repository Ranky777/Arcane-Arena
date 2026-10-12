// Fill out your copyright notice in the Description page of Project Settings.

#include "ArcaneLifeBarWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GAS/ArcaneAttributeSet.h"
#include "UI/ArcaneUITheme.h"

void UArcaneLifeBarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	TargetFill = CurrentFill = 1.0f;
	bLowHealth = false;

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

void UArcaneLifeBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (Bar == nullptr)
	{
		return;
	}

	CurrentFill = FMath::FInterpTo(CurrentFill, TargetFill, InDeltaTime, ArcaneUITheme::BarInterpSpeed);
	Bar->SetPercent(CurrentFill);

	// 低血量脉冲：在红橙色间呼吸，提示危急状态
	const bool bNowLow = CurrentFill <= ArcaneUITheme::LowHealthRatio && CurrentFill > 0.0f;
	if (bNowLow != bLowHealth)
	{
		bLowHealth = bNowLow;
		if (!bLowHealth)
		{
			NativeOnOwnerRelationSet(GetOwnerRelation());
		}
	}
	if (bLowHealth)
	{
		const float Pulse = 0.5f + 0.5f * FMath::Sin(GetWorld()->GetTimeSeconds() * 6.0f);
		Bar->SetFillColorAndOpacity(FMath::Lerp(ArcaneUITheme::HealthLow, ArcaneUITheme::HealthEnemy, Pulse));
	}
}
