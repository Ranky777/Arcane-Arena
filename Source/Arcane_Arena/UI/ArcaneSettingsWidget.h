// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ArcaneSettingsWidget.generated.h"

class UBorder;
class UButton;
class UComboBoxString;
class UPanelWidget;
class USlider;
class UTextBlock;

/**
 * 游戏设置面板（选项卡式，全部下拉框/滑条，即改即生效并持久化）：
 * 视频页 = 显示模式 / 分辨率 / 画质 / 垂直同步 / 帧率上限（UComboBoxString）
 * 音频页 = 主/音乐/音效音量（USlider → GameInstance 瞬态 SoundClass+SoundMix）
 * 游戏页 = 视野 FOV（USlider，本地相机即时应用）+ 按键提示
 * 持久化：显示模式/分辨率/VSync/帧率走 UGameUserSettings；画质走 scalability ini；
 * 音量/FOV 走 GameUserSettings.ini [ArcaneSettings]。
 *
 * 控件命名约定（BindWidgetOptional，WBP_Settings 缺失即降级）：
 * Border_Backdrop / Border_Card / Text_Title
 * Button_TabVideo / Button_TabAudio / Button_TabGame
 * Panel_Video / Panel_Audio / Panel_Game
 * Combo_DisplayMode / Combo_Resolution / Combo_Quality / Combo_VSync / Combo_FrameRate
 * Slider_Master / Slider_Music / Slider_SFX + Text_MasterValue / Text_MusicValue / Text_SFXValue
 * Slider_FOV + Text_FOVValue / Text_KeyHints / Button_Close
 */
UCLASS()
class ARCANE_ARENA_API UArcaneSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 面板弹出时回填当前实际设置
	UFUNCTION(BlueprintCallable, Category = "Arcane|Settings")
	void RefreshFromUserSettings();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// —— 底衬 / 标题 ——
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_Backdrop;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_Card;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Title;

	// —— 选项卡 ——
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_TabVideo;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_TabAudio;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_TabGame;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> Panel_Video;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> Panel_Audio;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> Panel_Game;

	// —— 视频页 ——
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> Combo_DisplayMode;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> Combo_Resolution;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> Combo_Quality;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> Combo_VSync;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> Combo_FrameRate;

	// —— 音频页 ——
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USlider> Slider_Master;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USlider> Slider_Music;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USlider> Slider_SFX;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_MasterValue;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_MusicValue;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_SFXValue;

	// —— 游戏页 ——
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USlider> Slider_FOV;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_FOVValue;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_KeyHints;

	// —— 底部 ——
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Close;

private:
	UFUNCTION()
	void HandleTabVideoClicked();

	UFUNCTION()
	void HandleTabAudioClicked();

	UFUNCTION()
	void HandleTabGameClicked();

	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleDisplayModeChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandleResolutionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandleQualityChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandleVSyncChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandleFrameRateChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandleMasterVolumeChanged(float Value);

	UFUNCTION()
	void HandleMusicVolumeChanged(float Value);

	UFUNCTION()
	void HandleSFXVolumeChanged(float Value);

	UFUNCTION()
	void HandleFOVChanged(float Value);

	void ShowTab(int32 Index);
	void BuildVideoOptions();
	void StyleAllWidgets();
	class UArcaneGameInstance* GetArcaneGI() const;

	int32 ActiveTab = 0;
	bool bProgrammatic = false;
	TArray<FIntPoint> SupportedResolutions;
};
