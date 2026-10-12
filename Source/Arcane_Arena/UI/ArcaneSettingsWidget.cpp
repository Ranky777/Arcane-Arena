// Fill out your copyright notice in the Description page of Project Settings.

#include "ArcaneSettingsWidget.h"

#include "ArcaneUITheme.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/PanelWidget.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Framework/ArcaneGameInstance.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Scalability.h"

namespace ArcaneSettingsPrivate
{
	static const TCHAR* QualityNames[] = {
		TEXT("低"), TEXT("中"), TEXT("高"), TEXT("史诗"), TEXT("电影")
	};
	static const int32 QualityCount = UE_ARRAY_COUNT(QualityNames);

	static const TCHAR* DisplayModeNames[] = {
		TEXT("全屏"), TEXT("无边框窗口"), TEXT("窗口化")
	};

	static const float FrameRateOptions[] = { 0.f, 60.f, 90.f, 120.f, 144.f, 240.f };

	static FString FrameRateLabel(float Limit)
	{
		return Limit <= 0.f ? FString(TEXT("不限")) : FString::Printf(TEXT("%d FPS"), FMath::RoundToInt(Limit));
	}

	/** 递归给面板内所有 TextBlock 套正文样式（标签层），值文本随后单独提亮 */
	void ApplyBodyTextsRecursive(UPanelWidget* Root, float Size, const FLinearColor& Color)
	{
		if (Root == nullptr)
		{
			return;
		}
		for (UWidget* Child : Root->GetAllChildren())
		{
			if (UTextBlock* T = Cast<UTextBlock>(Child))
			{
				ArcaneUIStyle::ApplyText(T, Size, Color);
			}
			else if (UPanelWidget* Sub = Cast<UPanelWidget>(Child))
			{
				ApplyBodyTextsRecursive(Sub, Size, Color);
			}
		}
	}
}

void UArcaneSettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	StyleAllWidgets();

	// —— 选项卡 ——
	if (Button_TabVideo) { Button_TabVideo->OnClicked.AddDynamic(this, &UArcaneSettingsWidget::HandleTabVideoClicked); }
	if (Button_TabAudio) { Button_TabAudio->OnClicked.AddDynamic(this, &UArcaneSettingsWidget::HandleTabAudioClicked); }
	if (Button_TabGame)  { Button_TabGame->OnClicked.AddDynamic(this, &UArcaneSettingsWidget::HandleTabGameClicked); }
	if (Button_Close)    { Button_Close->OnClicked.AddDynamic(this, &UArcaneSettingsWidget::HandleCloseClicked); }

	// —— 视频页下拉框 ——
	if (Combo_DisplayMode)
	{
		Combo_DisplayMode->ClearOptions();
		for (const TCHAR* Name : ArcaneSettingsPrivate::DisplayModeNames)
		{
			Combo_DisplayMode->AddOption(FString(Name));
		}
		Combo_DisplayMode->OnSelectionChanged.AddDynamic(this, &UArcaneSettingsWidget::HandleDisplayModeChanged);
	}
	if (Combo_Quality)
	{
		Combo_Quality->ClearOptions();
		for (const TCHAR* Name : ArcaneSettingsPrivate::QualityNames)
		{
			Combo_Quality->AddOption(FString(Name));
		}
		Combo_Quality->OnSelectionChanged.AddDynamic(this, &UArcaneSettingsWidget::HandleQualityChanged);
	}
	if (Combo_VSync)
	{
		Combo_VSync->ClearOptions();
		Combo_VSync->AddOption(FString(TEXT("开")));
		Combo_VSync->AddOption(FString(TEXT("关")));
		Combo_VSync->OnSelectionChanged.AddDynamic(this, &UArcaneSettingsWidget::HandleVSyncChanged);
	}
	if (Combo_FrameRate)
	{
		Combo_FrameRate->ClearOptions();
		for (const float Limit : ArcaneSettingsPrivate::FrameRateOptions)
		{
			Combo_FrameRate->AddOption(ArcaneSettingsPrivate::FrameRateLabel(Limit));
		}
		Combo_FrameRate->OnSelectionChanged.AddDynamic(this, &UArcaneSettingsWidget::HandleFrameRateChanged);
	}
	if (Combo_Resolution)
	{
		Combo_Resolution->OnSelectionChanged.AddDynamic(this, &UArcaneSettingsWidget::HandleResolutionChanged);
	}

	// —— 音频 / 游戏页滑条 ——
	if (Slider_Master) { Slider_Master->OnValueChanged.AddDynamic(this, &UArcaneSettingsWidget::HandleMasterVolumeChanged); }
	if (Slider_Music)  { Slider_Music->OnValueChanged.AddDynamic(this, &UArcaneSettingsWidget::HandleMusicVolumeChanged); }
	if (Slider_SFX)    { Slider_SFX->OnValueChanged.AddDynamic(this, &UArcaneSettingsWidget::HandleSFXVolumeChanged); }
	if (Slider_FOV)    { Slider_FOV->OnValueChanged.AddDynamic(this, &UArcaneSettingsWidget::HandleFOVChanged); }

	if (Text_KeyHints)
	{
		Text_KeyHints->SetText(NSLOCTEXT("ArcaneSettings", "KeyHints",
			"WASD 移动    左键 普通攻击    右键 蓄力攻击    Space 冲刺\n"
			"1 / 2 / 3 技能    Tab 计分板    Esc 暂停"));
	}

	RefreshFromUserSettings();
	ShowTab(0);
}

void UArcaneSettingsWidget::NativeDestruct()
{
	if (Button_TabVideo) { Button_TabVideo->OnClicked.RemoveDynamic(this, &UArcaneSettingsWidget::HandleTabVideoClicked); }
	if (Button_TabAudio) { Button_TabAudio->OnClicked.RemoveDynamic(this, &UArcaneSettingsWidget::HandleTabAudioClicked); }
	if (Button_TabGame)  { Button_TabGame->OnClicked.RemoveDynamic(this, &UArcaneSettingsWidget::HandleTabGameClicked); }
	if (Button_Close)    { Button_Close->OnClicked.RemoveDynamic(this, &UArcaneSettingsWidget::HandleCloseClicked); }

	if (Combo_DisplayMode)  { Combo_DisplayMode->OnSelectionChanged.RemoveDynamic(this, &UArcaneSettingsWidget::HandleDisplayModeChanged); }
	if (Combo_Resolution)   { Combo_Resolution->OnSelectionChanged.RemoveDynamic(this, &UArcaneSettingsWidget::HandleResolutionChanged); }
	if (Combo_Quality)      { Combo_Quality->OnSelectionChanged.RemoveDynamic(this, &UArcaneSettingsWidget::HandleQualityChanged); }
	if (Combo_VSync)        { Combo_VSync->OnSelectionChanged.RemoveDynamic(this, &UArcaneSettingsWidget::HandleVSyncChanged); }
	if (Combo_FrameRate)    { Combo_FrameRate->OnSelectionChanged.RemoveDynamic(this, &UArcaneSettingsWidget::HandleFrameRateChanged); }

	if (Slider_Master) { Slider_Master->OnValueChanged.RemoveDynamic(this, &UArcaneSettingsWidget::HandleMasterVolumeChanged); }
	if (Slider_Music)  { Slider_Music->OnValueChanged.RemoveDynamic(this, &UArcaneSettingsWidget::HandleMusicVolumeChanged); }
	if (Slider_SFX)    { Slider_SFX->OnValueChanged.RemoveDynamic(this, &UArcaneSettingsWidget::HandleSFXVolumeChanged); }
	if (Slider_FOV)    { Slider_FOV->OnValueChanged.RemoveDynamic(this, &UArcaneSettingsWidget::HandleFOVChanged); }

	Super::NativeDestruct();
}

void UArcaneSettingsWidget::StyleAllWidgets()
{
	// 全屏底衬：深色半透（保持方形覆盖，圆角只给卡片）
	if (Border_Backdrop)
	{
		Border_Backdrop->SetBrushColor(ArcaneUITheme::BgDeep);
	}
	if (Border_Card)
	{
		ArcaneUIStyle::ApplyPanel(Border_Card, FLinearColor(0.055f, 0.065f, 0.095f, 0.98f),
			ArcaneUITheme::RadiusPanel, 1.5f, ArcaneUITheme::OutlineSoft);
		Border_Card->SetPadding(FMargin(28.f, 22.f));
	}
	if (Text_Title)
	{
		Text_Title->SetText(NSLOCTEXT("ArcaneSettings", "Title", "游戏设置"));
		ArcaneUIStyle::ApplyTitle(Text_Title, ArcaneUITheme::FontH2);
		Text_Title->SetJustification(ETextJustify::Center);
	}

	ArcaneUIStyle::ApplyButton(Button_TabVideo, EArcaneButtonTone::Primary, ArcaneUITheme::FontBody);
	ArcaneUIStyle::ApplyButton(Button_TabAudio, EArcaneButtonTone::Ghost, ArcaneUITheme::FontBody);
	ArcaneUIStyle::ApplyButton(Button_TabGame, EArcaneButtonTone::Ghost, ArcaneUITheme::FontBody);

	ArcaneUIStyle::ApplyButton(Button_Close, EArcaneButtonTone::Secondary, ArcaneUITheme::FontBody);

	auto StyleComboRow = [](UPanelWidget* Page)
	{
		ArcaneUIStyle::ApplyRowSpacing(Page, ArcaneUITheme::PadRow);
		ArcaneSettingsPrivate::ApplyBodyTextsRecursive(Page, ArcaneUITheme::FontBody, ArcaneUITheme::TextSecondary);
	};
	StyleComboRow(Panel_Video);
	StyleComboRow(Panel_Audio);
	StyleComboRow(Panel_Game);

	UComboBoxString* Combos[] = { Combo_DisplayMode, Combo_Resolution, Combo_Quality, Combo_VSync, Combo_FrameRate };
	for (UComboBoxString* Combo : Combos)
	{
		ArcaneUIStyle::ApplyComboBox(Combo);
	}

	USlider* Sliders[] = { Slider_Master, Slider_Music, Slider_SFX, Slider_FOV };
	for (USlider* S : Sliders)
	{
		ArcaneUIStyle::ApplySlider(S);
	}

	UTextBlock* Values[] = { Text_MasterValue, Text_MusicValue, Text_SFXValue, Text_FOVValue };
	for (UTextBlock* V : Values)
	{
		if (V)
		{
			ArcaneUIStyle::ApplyText(V, ArcaneUITheme::FontBody, ArcaneUITheme::TextPrimary, false);
			V->SetJustification(ETextJustify::Center);
		}
	}
	if (Text_KeyHints)
	{
		ArcaneUIStyle::ApplyText(Text_KeyHints, ArcaneUITheme::FontBody, ArcaneUITheme::TextSecondary);
	}
}

void UArcaneSettingsWidget::ShowTab(int32 Index)
{
	ActiveTab = Index;
	const auto Vis = [](int32 Tab, int32 Self)
	{
		return Tab == Self ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	};
	if (Panel_Video) { Panel_Video->SetVisibility(Vis(Index, 0)); }
	if (Panel_Audio) { Panel_Audio->SetVisibility(Vis(Index, 1)); }
	if (Panel_Game)  { Panel_Game->SetVisibility(Vis(Index, 2)); }

	// 当前页的 Tab 高亮，其余 Ghost
	ArcaneUIStyle::ApplyButton(Button_TabVideo, Index == 0 ? EArcaneButtonTone::Primary : EArcaneButtonTone::Ghost, ArcaneUITheme::FontBody);
	ArcaneUIStyle::ApplyButton(Button_TabAudio, Index == 1 ? EArcaneButtonTone::Primary : EArcaneButtonTone::Ghost, ArcaneUITheme::FontBody);
	ArcaneUIStyle::ApplyButton(Button_TabGame, Index == 2 ? EArcaneButtonTone::Primary : EArcaneButtonTone::Ghost, ArcaneUITheme::FontBody);
}

void UArcaneSettingsWidget::BuildVideoOptions()
{
	SupportedResolutions.Empty();
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(SupportedResolutions);

	UGameUserSettings* GUS = UGameUserSettings::GetGameUserSettings();
	if (GUS && SupportedResolutions.Num() > 0)
	{
		const FIntPoint Current = GUS->GetScreenResolution();
		if (!SupportedResolutions.Contains(Current))
		{
			// 当前非全屏支持模式（窗口化）也要能在下拉里回显
			SupportedResolutions.Add(Current);
		}
	}
	else if (GUS)
	{
		SupportedResolutions.Add(GUS->GetDesktopResolution());
		SupportedResolutions.Add(FIntPoint(1600, 900));
		SupportedResolutions.Add(FIntPoint(1280, 720));
		SupportedResolutions.Add(FIntPoint(1024, 768));
	}

	if (Combo_Resolution)
	{
		Combo_Resolution->ClearOptions();
		for (const FIntPoint& R : SupportedResolutions)
		{
			Combo_Resolution->AddOption(FString::Printf(TEXT("%d × %d"), R.X, R.Y));
		}
	}
}

void UArcaneSettingsWidget::RefreshFromUserSettings()
{
	UGameUserSettings* GUS = UGameUserSettings::GetGameUserSettings();
	if (GUS == nullptr)
	{
		return;
	}

	BuildVideoOptions();

	bProgrammatic = true;

	// UComboBoxString 内部自动生成/更新选中项显示文本，无需外部 TextBlock
	if (Combo_DisplayMode)
	{
		const EWindowMode::Type Mode = GUS->GetFullscreenMode();
		const int32 Index = Mode == EWindowMode::Fullscreen ? 0 : Mode == EWindowMode::WindowedFullscreen ? 1 : 2;
		Combo_DisplayMode->SetSelectedOption(FString(ArcaneSettingsPrivate::DisplayModeNames[Index]));
	}
	if (Combo_Resolution)
	{
		const FIntPoint Current = GUS->GetScreenResolution();
		Combo_Resolution->SetSelectedOption(FString::Printf(TEXT("%d × %d"), Current.X, Current.Y));
	}
	if (Combo_Quality)
	{
		const int32 Level = FMath::Clamp(Scalability::GetQualityLevels().GetSingleQualityLevel(), 0, ArcaneSettingsPrivate::QualityCount - 1);
		Combo_Quality->SetSelectedOption(FString(ArcaneSettingsPrivate::QualityNames[Level]));
	}
	if (Combo_VSync)
	{
		Combo_VSync->SetSelectedOption(GUS->IsVSyncEnabled() ? FString(TEXT("开")) : FString(TEXT("关")));
	}
	if (Combo_FrameRate)
	{
		Combo_FrameRate->SetSelectedOption(ArcaneSettingsPrivate::FrameRateLabel(GUS->GetFrameRateLimit()));
	}

	if (UArcaneGameInstance* GI = GetArcaneGI())
	{
		auto FillVol = [GI](USlider* S, UTextBlock* T, FName Channel)
		{
			if (S == nullptr)
			{
				return;
			}
			const float V = GI->GetVolumeChannel(Channel);
			S->SetValue(V);
			if (T)
			{
				T->SetText(FText::AsPercent(V));
			}
		};
		FillVol(Slider_Master, Text_MasterValue, TEXT("Master"));
		FillVol(Slider_Music, Text_MusicValue, TEXT("Music"));
		FillVol(Slider_SFX, Text_SFXValue, TEXT("SFX"));

		if (Slider_FOV)
		{
			const float Fov = GI->GetPlayerFOV();
			Slider_FOV->SetValue((Fov - 70.f) / 40.f);
			if (Text_FOVValue)
			{
				Text_FOVValue->SetText(FText::FromString(FString::Printf(TEXT("%.0f"), Fov)));
			}
		}
	}

	bProgrammatic = false;
}

UArcaneGameInstance* UArcaneSettingsWidget::GetArcaneGI() const
{
	return GetGameInstance() ? Cast<UArcaneGameInstance>(GetGameInstance()) : nullptr;
}

// ————— 事件处理：全部即改即生效 + 落盘 —————

void UArcaneSettingsWidget::HandleDisplayModeChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (bProgrammatic)
	{
		return;
	}
	UGameUserSettings* GUS = UGameUserSettings::GetGameUserSettings();
	if (GUS == nullptr)
	{
		return;
	}
	const int32 Index = SelectedItem == FString(ArcaneSettingsPrivate::DisplayModeNames[0]) ? 0
		: SelectedItem == FString(ArcaneSettingsPrivate::DisplayModeNames[1]) ? 1 : 2;
	static const EWindowMode::Type Modes[] = { EWindowMode::Fullscreen, EWindowMode::WindowedFullscreen, EWindowMode::Windowed };
	GUS->SetFullscreenMode(Modes[Index]);
	GUS->ApplyResolutionSettings(false);
	GUS->SaveSettings();
}

void UArcaneSettingsWidget::HandleResolutionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (bProgrammatic)
	{
		return;
	}
	UGameUserSettings* GUS = UGameUserSettings::GetGameUserSettings();
	if (GUS == nullptr)
	{
		return;
	}
	// 解析 "W × H"
	TArray<FString> Parts;
	SelectedItem.ParseIntoArray(Parts, TEXT("×"), true);
	if (Parts.Num() == 2)
	{
		const int32 X = FCString::Atoi(*Parts[0].TrimStartAndEnd());
		const int32 Y = FCString::Atoi(*Parts[1].TrimStartAndEnd());
		if (X > 0 && Y > 0)
		{
			GUS->SetScreenResolution(FIntPoint(X, Y));
			GUS->ApplyResolutionSettings(false);
			GUS->SaveSettings();
		}
	}
}

void UArcaneSettingsWidget::HandleQualityChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (bProgrammatic)
	{
		return;
	}
	int32 Level = ArcaneSettingsPrivate::QualityCount - 1;
	for (int32 i = 0; i < ArcaneSettingsPrivate::QualityCount; ++i)
	{
		if (SelectedItem == FString(ArcaneSettingsPrivate::QualityNames[i]))
		{
			Level = i;
			break;
		}
	}
	// 引擎唯一推荐落点：SetQualityLevels 写好全部 sg.* cvar 并触发设备更新；
	// SaveState 持久化（sg.* 不会随 GameUserSettings::SaveSettings 落盘）。
	Scalability::FQualityLevels Levels = Scalability::GetQualityLevels();
	Levels.SetFromSingleQualityLevel(Level);
	Scalability::SetQualityLevels(Levels);
	Scalability::SaveState(GGameUserSettingsIni);
}

void UArcaneSettingsWidget::HandleVSyncChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (bProgrammatic)
	{
		return;
	}
	UGameUserSettings* GUS = UGameUserSettings::GetGameUserSettings();
	if (GUS == nullptr)
	{
		return;
	}
	GUS->SetVSyncEnabled(SelectedItem == FString(TEXT("开")));
	GUS->ApplyNonResolutionSettings();
	GUS->SaveSettings();
}

void UArcaneSettingsWidget::HandleFrameRateChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (bProgrammatic)
	{
		return;
	}
	UGameUserSettings* GUS = UGameUserSettings::GetGameUserSettings();
	if (GUS == nullptr)
	{
		return;
	}
	float Limit = 0.f;
	if (SelectedItem != FString(TEXT("不限")))
	{
		Limit = static_cast<float>(FCString::Atoi(*SelectedItem));
	}
	GUS->SetFrameRateLimit(Limit);
	GUS->SaveSettings();
}

void UArcaneSettingsWidget::HandleMasterVolumeChanged(float Value)
{
	if (UArcaneGameInstance* GI = GetArcaneGI())
	{
		GI->SetVolumeChannel(TEXT("Master"), Value);
	}
	if (Text_MasterValue)
	{
		Text_MasterValue->SetText(FText::AsPercent(Value));
	}
}

void UArcaneSettingsWidget::HandleMusicVolumeChanged(float Value)
{
	if (UArcaneGameInstance* GI = GetArcaneGI())
	{
		GI->SetVolumeChannel(TEXT("Music"), Value);
	}
	if (Text_MusicValue)
	{
		Text_MusicValue->SetText(FText::AsPercent(Value));
	}
}

void UArcaneSettingsWidget::HandleSFXVolumeChanged(float Value)
{
	if (UArcaneGameInstance* GI = GetArcaneGI())
	{
		GI->SetVolumeChannel(TEXT("SFX"), Value);
	}
	if (Text_SFXValue)
	{
		Text_SFXValue->SetText(FText::AsPercent(Value));
	}
}

void UArcaneSettingsWidget::HandleFOVChanged(float Value)
{
	const float Fov = 70.f + Value * 40.f;
	if (UArcaneGameInstance* GI = GetArcaneGI())
	{
		GI->SetPlayerFOV(Fov);
	}
	if (Text_FOVValue)
	{
		Text_FOVValue->SetText(FText::FromString(FString::Printf(TEXT("%.0f"), Fov)));
	}
}

void UArcaneSettingsWidget::HandleTabVideoClicked()
{
	ShowTab(0);
}

void UArcaneSettingsWidget::HandleTabAudioClicked()
{
	ShowTab(1);
}

void UArcaneSettingsWidget::HandleTabGameClicked()
{
	ShowTab(2);
}

void UArcaneSettingsWidget::HandleCloseClicked()
{
	RemoveFromParent();
}
