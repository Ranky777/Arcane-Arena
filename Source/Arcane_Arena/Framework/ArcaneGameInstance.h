// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "ArcaneGameInstance.generated.h"

class UArcaneCombatData;
class USoundClass;
class USoundMix;

/**
 * GameInstance：跨关卡存活的全局宿主。
 * ① 战斗数据注册表（CombatDataAsset）在此加载并硬引用防 GC；
 * ② 玩家本地设置（音量三通道 / 视野 FOV）在此持久化到
 *    GameUserSettings.ini [ArcaneSettings]，并提供即改即生效的应用入口。
 */
UCLASS()
class ARCANE_ARENA_API UArcaneGameInstance : public UGameInstance
{
	GENERATED_BODY()
	
public:
	virtual void Init() override;

	// —— 音量通道：Master/Music/SFX，0..1（可超 1 至 2）。经瞬态 SoundClass +
	// SoundMix 覆盖实现；声音资产把 SoundClass 指到这三个类即受控。
	UFUNCTION(BlueprintCallable, Category = "Arcane|Settings")
	void SetVolumeChannel(FName Channel, float Volume);

	UFUNCTION(BlueprintCallable, Category = "Arcane|Settings")
	float GetVolumeChannel(FName Channel) const;

	// —— 视野 FOV：70..110，本地角色相机在 BeginPlay 读取 ——
	UFUNCTION(BlueprintCallable, Category = "Arcane|Settings")
	void SetPlayerFOV(float NewFOV);

	UFUNCTION(BlueprintCallable, Category = "Arcane|Settings")
	float GetPlayerFOV() const { return PlayerFOV; }

	// 供角色拿瞬态 SoundClass（未来给声音资产指派类别时用）
	const USoundClass* GetMusicSoundClass() const { return ClsMusic; }
	const USoundClass* GetSFXSoundClass() const { return ClsSFX; }

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Combat Data")
	TSoftObjectPtr<UArcaneCombatData> CombatDataAsset;

	// 硬引用根住加载后的注册表：软引用不阻止 GC，ServerTravel 触发的回收会让
	// UArcaneCombatData::Singleton 悬垂，客户端开局放技能即访问已释放内存闪退。
	UPROPERTY()
	TObjectPtr<UArcaneCombatData> LoadedCombatData;

private:
	UPROPERTY()
	TObjectPtr<USoundClass> ClsMaster;
	UPROPERTY()
	TObjectPtr<USoundClass> ClsMusic;
	UPROPERTY()
	TObjectPtr<USoundClass> ClsSFX;
	UPROPERTY()
	TObjectPtr<USoundMix> VolumeMix;

	float VolumeMaster = 1.0f;
	float VolumeMusic = 1.0f;
	float VolumeSFX = 1.0f;
	float PlayerFOV = 90.0f;

	void LoadLocalSettings();
	void ApplyVolumeOverrides(float FadeIn = 0.1f);
};
