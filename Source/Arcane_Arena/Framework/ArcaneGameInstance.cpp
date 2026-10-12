// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneGameInstance.h"

#include "Camera/CameraComponent.h"
#include "Combat/ArcaneCombatData.h"
#include "Components.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

void UArcaneGameInstance::Init()
{
	Super::Init();

	// 本地设置与瞬态音频类：与战斗数据无关，先行初始化
	LoadLocalSettings();
	
	if (CombatDataAsset.IsNull())
	{
		ensureAlwaysMsgf(false,
			TEXT("CombatDataAsset 未设置，请在 BP_ArcaneGameInstance 的 Class Defaults 中指定 DT_CombatData。"));
		return;
	}
	
	const UArcaneCombatData* Registry = CombatDataAsset.LoadSynchronous();
	
	if (Registry != nullptr)
	{
		// 存一份硬引用防 GC（GameInstance 跨 ServerTravel 存活，能全程根住该资产）
		LoadedCombatData = const_cast<UArcaneCombatData*>(Registry);
		UArcaneCombatData::SetRegistry(Registry);
	}
	else
	{
		ensureAlwaysMsgf(false,
			TEXT("CombatDataAsset 加载失败，请检查 GameInstance 引用的 DT_CombatData 资产。"));
	}
}

void UArcaneGameInstance::LoadLocalSettings()
{
	// 瞬态声音类别三件套 + 常驻 Mix：音量滑条经 SetSoundMixClassOverride 即时生效。
	// 无资产依赖，Outer=GameInstance 即终身防 GC。
	ClsMaster = NewObject<USoundClass>(this, TEXT("ArcaneSoundClass_Master"));
	ClsMusic = NewObject<USoundClass>(this, TEXT("ArcaneSoundClass_Music"));
	ClsSFX = NewObject<USoundClass>(this, TEXT("ArcaneSoundClass_SFX"));
	VolumeMix = NewObject<USoundMix>(this, TEXT("ArcaneVolumeMix"));

	if (GConfig)
	{
		GConfig->GetFloat(TEXT("ArcaneSettings"), TEXT("MasterVolume"), VolumeMaster, GGameUserSettingsIni);
		GConfig->GetFloat(TEXT("ArcaneSettings"), TEXT("MusicVolume"), VolumeMusic, GGameUserSettingsIni);
		GConfig->GetFloat(TEXT("ArcaneSettings"), TEXT("SFXVolume"), VolumeSFX, GGameUserSettingsIni);
		GConfig->GetFloat(TEXT("ArcaneSettings"), TEXT("PlayerFOV"), PlayerFOV, GGameUserSettingsIni);
	}

	UGameplayStatics::SetBaseSoundMix(this, VolumeMix);
	ApplyVolumeOverrides(0.f);
}

void UArcaneGameInstance::ApplyVolumeOverrides(float FadeIn)
{
	if (VolumeMix == nullptr)
	{
		return;
	}
	UGameplayStatics::SetSoundMixClassOverride(this, VolumeMix, ClsMaster, VolumeMaster, 1.f, FadeIn, false);
	UGameplayStatics::SetSoundMixClassOverride(this, VolumeMix, ClsMusic, VolumeMusic, 1.f, FadeIn, false);
	UGameplayStatics::SetSoundMixClassOverride(this, VolumeMix, ClsSFX, VolumeSFX, 1.f, FadeIn, false);
}

void UArcaneGameInstance::SetVolumeChannel(FName Channel, float Volume)
{
	Volume = FMath::Clamp(Volume, 0.f, 2.f);
	if (Channel == TEXT("Master"))
	{
		VolumeMaster = Volume;
		GConfig->SetFloat(TEXT("ArcaneSettings"), TEXT("MasterVolume"), Volume, GGameUserSettingsIni);
	}
	else if (Channel == TEXT("Music"))
	{
		VolumeMusic = Volume;
		GConfig->SetFloat(TEXT("ArcaneSettings"), TEXT("MusicVolume"), Volume, GGameUserSettingsIni);
	}
	else if (Channel == TEXT("SFX"))
	{
		VolumeSFX = Volume;
		GConfig->SetFloat(TEXT("ArcaneSettings"), TEXT("SFXVolume"), Volume, GGameUserSettingsIni);
	}
	else
	{
		return;
	}
	GConfig->Flush(false, GGameUserSettingsIni);
	ApplyVolumeOverrides();
}

float UArcaneGameInstance::GetVolumeChannel(FName Channel) const
{
	if (Channel == TEXT("Master")) return VolumeMaster;
	if (Channel == TEXT("Music"))  return VolumeMusic;
	if (Channel == TEXT("SFX"))    return VolumeSFX;
	return 1.f;
}

void UArcaneGameInstance::SetPlayerFOV(float NewFOV)
{
	PlayerFOV = FMath::Clamp(NewFOV, 70.f, 110.f);
	GConfig->SetFloat(TEXT("ArcaneSettings"), TEXT("PlayerFOV"), PlayerFOV, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);

	// 即改即生效：应用到当前所有可控 Pawn 的相机（新 Pawn 由 BeginPlay 补）
	if (UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* PC = It->Get();
			if (PC == nullptr || PC->GetPawn() == nullptr)
			{
				continue;
			}
			TInlineComponentArray<UCameraComponent*> Cameras;
			PC->GetPawn()->GetComponents(Cameras);
			for (UCameraComponent* Cam : Cameras)
			{
				Cam->SetFieldOfView(PlayerFOV);
			}
		}
	}
}
