// Fill out your copyright notice in the Description page of Project Settings.

#include "ArcaneUITheme.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace ArcaneUIStyle
{
	FLinearColor Lighten(const FLinearColor& C, float Amount)
	{
		return FLinearColor(
			FMath::Clamp(C.R + Amount, 0.f, 1.f),
			FMath::Clamp(C.G + Amount, 0.f, 1.f),
			FMath::Clamp(C.B + Amount, 0.f, 1.f),
			C.A);
	}

	FLinearColor Darken(const FLinearColor& C, float Amount)
	{
		const float K = FMath::Clamp(1.f - Amount, 0.f, 1.f);
		return FLinearColor(C.R * K, C.G * K, C.B * K, C.A);
	}

	FSlateBrush MakeRoundedBrush(float Radius, const FLinearColor& Tint, float OutlineWidth, const FLinearColor& OutlineColor)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Tint);
		if (Radius >= 0.f)
		{
			Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
			Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
		}
		else
		{
			// -1 = 药丸形：圆角恒为半高
			Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
		}
		Brush.OutlineSettings.Width = OutlineWidth;
		Brush.OutlineSettings.Color = FSlateColor(OutlineColor);
		Brush.ImageSize = FVector2D(16.f, 16.f);
		return Brush;
	}

	void GetButtonColors(EArcaneButtonTone Tone, FLinearColor& OutNormal, FLinearColor& OutHover, FLinearColor& OutPressed)
	{
		switch (Tone)
		{
		case EArcaneButtonTone::Primary:
			OutNormal = ArcaneUITheme::Accent;   OutHover = ArcaneUITheme::AccentHover;  OutPressed = ArcaneUITheme::AccentPressed; break;
		case EArcaneButtonTone::Danger:
			OutNormal = ArcaneUITheme::Danger;   OutHover = ArcaneUITheme::DangerHover;  OutPressed = ArcaneUITheme::DangerPressed; break;
		case EArcaneButtonTone::Success:
			OutNormal = ArcaneUITheme::Success;  OutHover = ArcaneUITheme::SuccessHover; OutPressed = ArcaneUITheme::SuccessPressed; break;
		case EArcaneButtonTone::Ghost:
			OutNormal = FLinearColor::Transparent;
			OutHover = FLinearColor(ArcaneUITheme::Accent.R, ArcaneUITheme::Accent.G, ArcaneUITheme::Accent.B, 0.20f);
			OutPressed = FLinearColor(ArcaneUITheme::Accent.R, ArcaneUITheme::Accent.G, ArcaneUITheme::Accent.B, 0.34f);
			break;
		default: // Secondary
			OutNormal = ArcaneUITheme::Neutral;  OutHover = ArcaneUITheme::NeutralHover; OutPressed = ArcaneUITheme::NeutralPressed; break;
		}
	}

	static void ApplyButtonCore(UButton* Btn, const FLinearColor& Normal, const FLinearColor& Hover,
		const FLinearColor& Pressed, bool bGhostOutline, float FontSize, float Radius)
	{
		const float OutlineW = bGhostOutline ? ArcaneUITheme::OutlineW : 1.f;
		const FLinearColor Outline = bGhostOutline
			? FLinearColor(ArcaneUITheme::Accent.R, ArcaneUITheme::Accent.G, ArcaneUITheme::Accent.B, 0.45f)
			: ArcaneUITheme::OutlineSoft;
		const FLinearColor HoverOutline = bGhostOutline
			? ArcaneUITheme::Accent
			: ArcaneUITheme::OutlineAccent;

		FButtonStyle Style;
		Style.Normal = MakeRoundedBrush(Radius, Normal, OutlineW, Outline);
		Style.Hovered = MakeRoundedBrush(Radius, Hover, OutlineW, HoverOutline);
		Style.Pressed = MakeRoundedBrush(Radius, Pressed, OutlineW, Outline);
		Style.Disabled = MakeRoundedBrush(Radius, FLinearColor(0.30f, 0.32f, 0.38f, 0.10f), 1.f, FLinearColor::Transparent);
		Style.NormalForeground = FSlateColor(ArcaneUITheme::TextPrimary);
		Style.HoveredForeground = FSlateColor(FLinearColor::White);
		Style.PressedForeground = FSlateColor(FLinearColor(0.85f, 0.88f, 0.95f));
		Style.DisabledForeground = FSlateColor(ArcaneUITheme::TextSecondary);
		Style.NormalPadding = FMargin(ArcaneUITheme::PadButtonH, ArcaneUITheme::PadButtonV);
		Style.PressedPadding = FMargin(ArcaneUITheme::PadButtonH, ArcaneUITheme::PadButtonV);
		Btn->SetStyle(Style);
		// 三态色已烘焙进画刷 Tint；SButton 最终色 = Tint × BackgroundColor，故底色置白
		Btn->SetBackgroundColor(FLinearColor::White);

		for (UWidget* Child : Btn->GetAllChildren())
		{
			if (UTextBlock* Label = Cast<UTextBlock>(Child))
			{
				ApplyText(Label, FontSize, ArcaneUITheme::TextPrimary, false);
				Label->SetJustification(ETextJustify::Center);
			}
		}
	}

	void ApplyButton(UButton* Btn, EArcaneButtonTone Tone, float FontSize, float Radius)
	{
		if (Btn == nullptr)
		{
			return;
		}
		FLinearColor Normal, Hover, Pressed;
		GetButtonColors(Tone, Normal, Hover, Pressed);
		ApplyButtonCore(Btn, Normal, Hover, Pressed, Tone == EArcaneButtonTone::Ghost, FontSize, Radius);
	}

	void ApplyButton(UButton* Btn, const FLinearColor& Base, float FontSize, float Radius)
	{
		if (Btn == nullptr)
		{
			return;
		}
		if (Base.A < KINDA_SMALL_NUMBER)
		{
			// 透明底 = Ghost 语义
			FLinearColor N, H, P;
			GetButtonColors(EArcaneButtonTone::Ghost, N, H, P);
			ApplyButtonCore(Btn, N, H, P, true, FontSize, Radius);
			return;
		}
		ApplyButtonCore(Btn, Base, Lighten(Base), Darken(Base), false, FontSize, Radius);
	}

	void ApplyText(UTextBlock* Text, float Size, const FLinearColor& Color, bool bShadow)
	{
		if (Text == nullptr)
		{
			return;
		}
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = FMath::RoundToInt32(Size);
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		if (bShadow)
		{
			Text->SetShadowColorAndOpacity(ArcaneUITheme::TextShadow);
			Text->SetShadowOffset(FVector2D(1.0, 1.5));
		}
		else
		{
			Text->SetShadowColorAndOpacity(FLinearColor::Transparent);
		}
	}

	void ApplyTitle(UTextBlock* Text, float Size)
	{
		if (Text == nullptr)
		{
			return;
		}
		ApplyText(Text, Size, ArcaneUITheme::TextGlow);
		Text->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.02f, 0.06f, 0.9f));
		Text->SetShadowOffset(FVector2D(2.0, 3.0));
	}

	void ApplyRowSpacing(UPanelWidget* Box, float Spacing)
	{
		// 5.8 UVerticalBox 无整体 Spacing API：逐子项槽位加半高 padding 等效
		if (Box == nullptr)
		{
			return;
		}
		for (int32 i = 0; i < Box->GetChildrenCount(); ++i)
		{
			if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(Box->GetChildAt(i)->Slot))
			{
				Slot->SetPadding(FMargin(0.f, Spacing * 0.5f, 0.f, Spacing * 0.5f));
			}
		}
	}

	void ApplyCard(UPanelWidget* Panel)
	{
		if (UBorder* Border = Cast<UBorder>(Panel))
		{
			ApplyPanel(Border, ArcaneUITheme::CardBg, ArcaneUITheme::RadiusCard, 1.f, ArcaneUITheme::OutlineSoft);
			Border->SetPadding(FMargin(ArcaneUITheme::PadCardH, ArcaneUITheme::PadCardH * 0.8f));
		}
	}

	void ApplyPanel(UBorder* Border, const FLinearColor& Bg, float Radius, float OutlineWidth, const FLinearColor& OutlineColor)
	{
		if (Border == nullptr)
		{
			return;
		}
		// SBorder 最终色 = Brush.Tint × BrushColor，BrushColor 保持白即可由 Bg 全权决定
		Border->SetBrush(MakeRoundedBrush(Radius, Bg, OutlineWidth, OutlineColor));
		Border->SetBrushColor(FLinearColor::White);
	}

	void ApplyComboBox(UComboBoxString* Combo)
	{
		if (Combo == nullptr)
		{
			return;
		}

		FLinearColor Normal, Hover, Pressed;
		GetButtonColors(EArcaneButtonTone::Secondary, Normal, Hover, Pressed);

		FComboBoxStyle Style;
		FButtonStyle& BtnStyle = Style.ComboButtonStyle.ButtonStyle;
		BtnStyle.Normal = MakeRoundedBrush(ArcaneUITheme::RadiusChip, Normal, 1.f, ArcaneUITheme::OutlineSoft);
		BtnStyle.Hovered = MakeRoundedBrush(ArcaneUITheme::RadiusChip, Hover, 1.f, ArcaneUITheme::OutlineAccent);
		BtnStyle.Pressed = MakeRoundedBrush(ArcaneUITheme::RadiusChip, Pressed, 1.f, ArcaneUITheme::OutlineAccent);
		BtnStyle.Disabled = MakeRoundedBrush(ArcaneUITheme::RadiusChip, FLinearColor(0.3f, 0.32f, 0.38f, 0.10f));
		BtnStyle.NormalForeground = FSlateColor(ArcaneUITheme::TextPrimary);
		BtnStyle.HoveredForeground = FSlateColor(FLinearColor::White);
		Style.ComboButtonStyle.MenuBorderBrush = MakeRoundedBrush(ArcaneUITheme::RadiusCard, ArcaneUITheme::BgDeep, 1.5f, ArcaneUITheme::OutlineAccent);
		Style.ComboButtonStyle.MenuBorderPadding = FMargin(4.f);
		Style.ComboButtonStyle.ShadowColorAndOpacity = FLinearColor(0.f, 0.f, 0.f, 0.55f);
		Style.ComboButtonStyle.ShadowOffset = FVector2D(0.f, 6.f);
		Style.ComboButtonStyle.ContentPadding = FMargin(ArcaneUITheme::PadButtonH * 0.5f, 6.f);
		Combo->SetWidgetStyle(Style);

		FTableRowStyle ItemStyle;
		const FLinearColor Clear(0.f, 0.f, 0.f, 0.f);
		ItemStyle.EvenRowBackgroundBrush = MakeRoundedBrush(ArcaneUITheme::RadiusChip, Clear);
		ItemStyle.OddRowBackgroundBrush = MakeRoundedBrush(ArcaneUITheme::RadiusChip, Clear);
		const FLinearColor HoverRow(ArcaneUITheme::Accent.R, ArcaneUITheme::Accent.G, ArcaneUITheme::Accent.B, 0.24f);
		ItemStyle.EvenRowBackgroundHoveredBrush = MakeRoundedBrush(ArcaneUITheme::RadiusChip, HoverRow);
		ItemStyle.OddRowBackgroundHoveredBrush = MakeRoundedBrush(ArcaneUITheme::RadiusChip, HoverRow);
		ItemStyle.ActiveBrush = ItemStyle.ActiveHoveredBrush = ItemStyle.InactiveBrush = ItemStyle.InactiveHoveredBrush =
			MakeRoundedBrush(ArcaneUITheme::RadiusChip, ArcaneUITheme::Accent);
		ItemStyle.SelectorFocusedBrush = MakeRoundedBrush(ArcaneUITheme::RadiusChip, FLinearColor(0.25f, 0.55f, 0.95f, 0.35f), 1.f, ArcaneUITheme::AccentHover);
		ItemStyle.TextColor = FSlateColor(ArcaneUITheme::TextPrimary);
		ItemStyle.SelectedTextColor = FSlateColor(FLinearColor::White);
		Combo->SetItemStyle(ItemStyle);
	}

	void ApplySlider(USlider* Slider)
	{
		if (Slider == nullptr)
		{
			return;
		}

		FSliderStyle Style;
		FSlateBrush Bar = MakeRoundedBrush(-1.f, FLinearColor(0.22f, 0.25f, 0.32f, 0.85f));
		Bar.ImageSize = FVector2D(12.f, 6.f);
		Style.NormalBarImage = Bar;
		FSlateBrush BarHover = Bar;
		BarHover.TintColor = FSlateColor(FLinearColor(0.30f, 0.34f, 0.44f, 0.95f));
		Style.HoveredBarImage = BarHover;
		Style.DisabledBarImage = Bar;

		FSlateBrush Thumb = MakeRoundedBrush(-1.f, ArcaneUITheme::TextPrimary);
		Thumb.ImageSize = FVector2D(18.f, 18.f);
		Style.NormalThumbImage = Thumb;
		Style.HoveredThumbImage = MakeRoundedBrush(-1.f, ArcaneUITheme::AccentHover, 1.5f, FLinearColor::White);
		FSlateBrush ThumbDisabled = Thumb;
		ThumbDisabled.TintColor = FSlateColor(ArcaneUITheme::TextSecondary);
		Style.DisabledThumbImage = ThumbDisabled;

		Slider->SetWidgetStyle(Style);
	}

	void ApplyProgressBar(UProgressBar* Bar)
	{
		if (Bar == nullptr)
		{
			return;
		}

		FProgressBarStyle Style;
		Style.BackgroundImage = MakeRoundedBrush(-1.f, ArcaneUITheme::BarBack);
		// SProgressBar 最终色 = FillImage.Tint × FillColorAndOpacity：填充刷保持白，颜色由调用方动态设
		Style.FillImage = MakeRoundedBrush(-1.f, FLinearColor::White);
		Style.EnableFillAnimation = false;
		Bar->SetWidgetStyle(Style);
	}

	void ApplyRowButton(UButton* Btn)
	{
		if (Btn == nullptr)
		{
			return;
		}

		const FLinearColor Clear(0.f, 0.f, 0.f, 0.f);
		FButtonStyle Style;
		Style.Normal = MakeRoundedBrush(ArcaneUITheme::RadiusChip, Clear);
		Style.Hovered = MakeRoundedBrush(ArcaneUITheme::RadiusChip, FLinearColor(ArcaneUITheme::Accent.R, ArcaneUITheme::Accent.G, ArcaneUITheme::Accent.B, 0.14f),
			1.f, ArcaneUITheme::OutlineSoft);
		Style.Pressed = MakeRoundedBrush(ArcaneUITheme::RadiusChip, FLinearColor(ArcaneUITheme::Accent.R, ArcaneUITheme::Accent.G, ArcaneUITheme::Accent.B, 0.28f),
			1.f, ArcaneUITheme::OutlineAccent);
		Style.Disabled = MakeRoundedBrush(ArcaneUITheme::RadiusChip, FLinearColor(0.1f, 0.1f, 0.12f, 0.30f));
		Style.NormalForeground = FSlateColor(ArcaneUITheme::TextPrimary);
		Style.HoveredForeground = FSlateColor(FLinearColor::White);
		Style.NormalPadding = FMargin(12.f, 8.f);
		Style.PressedPadding = FMargin(12.f, 8.f);
		Btn->SetStyle(Style);
		Btn->SetBackgroundColor(FLinearColor::White);
	}
}
