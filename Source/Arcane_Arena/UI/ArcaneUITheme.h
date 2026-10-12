// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"

class UBorder;
class UButton;
class UComboBoxString;
class UPanelWidget;
class UProgressBar;
class USlider;
class UTextBlock;

/**
 * 全局 UI 主题：设计令牌（Design Token）收口处。
 * 字体层级 / 间距 / 圆角 / 配色全部在此定义，控件样式统一由
 * ArcaneUIStyle 辅助函数在 NativeConstruct 应用，WBP 只放结构。
 * 改风格 = 改这一处（正式项目的 Theme 文件做法）。
 *
 * 5.8 起 FSlateBrush 支持 RoundedBox 矢量绘制（圆角+描边），
 * 全部面板/按钮/进度条零贴图，运行时纯代码着色。
 */
namespace ArcaneUITheme
{
	// —— 配色（深灰蓝底 + 电光蓝强调，竞技游戏通用语汇）——
	inline const FLinearColor BgDeep(0.035f, 0.045f, 0.07f, 0.96f);      // 面板底衬
	inline const FLinearColor CardBg(0.07f, 0.085f, 0.12f, 0.96f);       // 卡片底
	inline const FLinearColor Accent(0.16f, 0.55f, 0.95f);               // 主强调（蓝）
	inline const FLinearColor AccentHover(0.28f, 0.68f, 1.00f);
	inline const FLinearColor AccentPressed(0.10f, 0.38f, 0.72f);
	inline const FLinearColor Danger(0.86f, 0.25f, 0.20f);
	inline const FLinearColor DangerHover(0.98f, 0.38f, 0.32f);
	inline const FLinearColor DangerPressed(0.62f, 0.14f, 0.11f);
	inline const FLinearColor Success(0.25f, 0.72f, 0.40f);
	inline const FLinearColor SuccessHover(0.34f, 0.84f, 0.50f);
	inline const FLinearColor SuccessPressed(0.15f, 0.50f, 0.27f);
	inline const FLinearColor Neutral(0.13f, 0.15f, 0.20f);              // 次级按钮底
	inline const FLinearColor NeutralHover(0.19f, 0.22f, 0.29f);
	inline const FLinearColor NeutralPressed(0.09f, 0.10f, 0.14f);
	inline const FLinearColor TeamBlue(0.13f, 0.42f, 0.85f);
	inline const FLinearColor TeamRed(0.86f, 0.25f, 0.20f);

	// —— 文本层级 ——
	inline const FLinearColor TextPrimary(0.93f, 0.95f, 0.99f);
	inline const FLinearColor TextSecondary(0.62f, 0.66f, 0.74f);
	inline const FLinearColor TextGlow(1.f, 0.92f, 0.60f);
	inline const FLinearColor TextShadow(0.f, 0.f, 0.f, 0.72f);

	// —— 描边 / 分隔 ——
	inline const FLinearColor OutlineSoft(1.f, 1.f, 1.f, 0.10f);          // 卡片微弱高光边
	inline const FLinearColor OutlineAccent(0.35f, 0.72f, 1.f, 0.55f);    // 聚焦/选中强调边
	inline const FLinearColor BarBack(0.02f, 0.025f, 0.04f, 0.92f);       // 进度条底槽

	// 字号（点）
	inline const float FontH1 = 46.f;     // 大标题
	inline const float FontH2 = 26.f;     // 面板标题
	inline const float FontBody = 17.f;   // 正文 / 按钮
	inline const float FontSmall = 13.f;  // 辅助说明

	// —— 圆角（Slate 单位）——
	inline const float RadiusPanel = 14.f;
	inline const float RadiusCard  = 10.f;
	inline const float RadiusButton = 8.f;
	inline const float RadiusChip  = 6.f;
	inline const float OutlineW = 1.5f;

	// —— 间距（8pt 网格）——
	inline const float PadRow = 10.f;     // 行间
	inline const float PadCardH = 28.f;   // 卡片横向内边距
	inline const float PadButtonV = 10.f;
	inline const float PadButtonH = 24.f;

	// —— 头顶血条 ——
	inline const FLinearColor HealthSelf(0.35f, 0.85f, 0.42f);
	inline const FLinearColor HealthAlly(0.25f, 0.65f, 1.00f);
	inline const FLinearColor HealthEnemy(0.95f, 0.30f, 0.24f);
	inline const FLinearColor HealthLow(1.00f, 0.45f, 0.15f);
	inline const float LowHealthRatio = 0.30f;

	// —— 能量（蓝条）——
	inline const FLinearColor Energy(0.35f, 0.72f, 1.00f);

	// —— 技能槽 ——
	inline const FLinearColor SlotIdle(1.f, 1.f, 1.f, 1.f);
	inline const FLinearColor SlotCooling(0.05f, 0.06f, 0.09f, 0.72f);
	inline const FLinearColor SlotNoEnergy(0.10f, 0.10f, 0.12f, 0.55f);

	// —— 插值速度（每秒）——
	inline const float BarInterpSpeed = 10.0f;
	inline const float EnergyInterpSpeed = 8.0f;
}

/** 按钮色调：主行动 / 次级 / 危险 / 成功 / 幽灵（透明底） */
enum class EArcaneButtonTone : uint8
{
	Primary,
	Secondary,
	Danger,
	Success,
	Ghost,
};

/**
 * 运行时样式助手：把主题令牌应用到 UMG 控件。
 * 全部基于 RoundedBox 矢量画刷：统一圆角 + 微描边 + 状态色反馈，无贴图依赖。
 */
namespace ArcaneUIStyle
{
	/** 生成圆角矢量画刷；Radius<0 表示药丸形（半高圆角） */
	FLinearColor Lighten(const FLinearColor& C, float Amount = 0.18f);
	FLinearColor Darken(const FLinearColor& C, float Amount = 0.22f);
	FSlateBrush MakeRoundedBrush(float Radius, const FLinearColor& Tint,
		float OutlineWidth = 0.f, const FLinearColor& OutlineColor = FLinearColor::Transparent);

	/** 按钮色调 → 三态底色 */
	void GetButtonColors(EArcaneButtonTone Tone, FLinearColor& OutNormal,
		FLinearColor& OutHover, FLinearColor& OutPressed);

	/** 主按钮：圆角三态底 + 白色前景 + 统一内边距，Content 里的 TextBlock 自动套字号 */
	void ApplyButton(::UButton* Btn, EArcaneButtonTone Tone, float FontSize = ArcaneUITheme::FontBody,
		float Radius = ArcaneUITheme::RadiusButton);

	/** 自定义底色按钮：Hover/Pressed 由底色自动提亮/压暗（战队色、活动色等） */
	void ApplyButton(::UButton* Btn, const FLinearColor& Base, float FontSize = ArcaneUITheme::FontBody,
		float Radius = ArcaneUITheme::RadiusButton);

	/** 文本：按层级设字号与颜色，附投影 */
	void ApplyText(::UTextBlock* Text, float Size, const FLinearColor& Color, bool bShadow = true);

	/** 大标题：H1 + 金色发光 + 投影（菜单/结算标题统一入口） */
	void ApplyTitle(::UTextBlock* Text, float Size = ArcaneUITheme::FontH1);

	/** 垂直容器：统一行间距 */
	void ApplyRowSpacing(::UPanelWidget* Box, float Spacing);

	/** 卡片容器（UBorder）：圆角底 + 微描边 + 内边距 */
	void ApplyCard(::UPanelWidget* Panel);

	/** 任意 Border 套圆角底 */
	void ApplyPanel(::UBorder* Border, const FLinearColor& Bg, float Radius = ArcaneUITheme::RadiusPanel,
		float OutlineWidth = ArcaneUITheme::OutlineW,
		const FLinearColor& OutlineColor = ArcaneUITheme::OutlineSoft);

	/** 下拉框：圆角按钮态 + 深色圆角弹层 + 行高亮（字体在 WBP 里设，运行时无 setter） */
	void ApplyComboBox(::UComboBoxString* Combo);

	/** 滑条：药丸底槽 + 圆形滑块 */
	void ApplySlider(::USlider* Slider);

	/** 进度条：药丸底槽 + 药丸填充（填充色仍由调用方动态设置） */
	void ApplyProgressBar(::UProgressBar* Bar);

	/** 行按钮（会话列表项等）：整行可点的扁平圆角 */
	void ApplyRowButton(::UButton* Btn);
}
