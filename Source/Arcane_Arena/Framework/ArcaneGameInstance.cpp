// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneGameInstance.h"

#include "Combat/ArcaneCombatData.h"

void UArcaneGameInstance::Init()
{
	Super::Init();
	
	if (CombatDataAsset.IsNull())
	{
		ensureAlwaysMsgf(false,
			TEXT("CombatDataAsset 未设置，请在 BP_ArcaneGameInstance 的 Class Defaults 中指定 DT_CombatData。"));
		return;
	}
	
	const UArcaneCombatData* Registry = CombatDataAsset.LoadSynchronous();
	
	if (Registry != nullptr)
	{
		UArcaneCombatData::SetRegistry(Registry);
	}
	else
	{
		ensureAlwaysMsgf(false,
			TEXT("CombatDataAsset 加载失败，请检查 GameInstance 引用的 DT_CombatData 资产。"));
	}
}
