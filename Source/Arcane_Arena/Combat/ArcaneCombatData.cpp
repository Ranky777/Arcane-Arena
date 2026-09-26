#include "ArcaneCombatData.h"


const UArcaneCombatData* UArcaneCombatData::Singleton = nullptr;

const UArcaneCombatData* UArcaneCombatData::Get()
{
	if (Singleton == nullptr)
	{
		// 注册表必须在启动时由 GameInstance（或 Asset Manager）显式设置。
		// 这里不做硬编码路径兜底，避免架构上的字符串耦合与静默失效。
		ensureAlwaysMsgf(false,
			TEXT("UArcaneCombatData::Get() 调用时注册表未初始化！"
				 "请在 GameInstance::Init 中调用 SetRegistry(CombatDataAsset)。"));
	}
	
	return Singleton;
}


