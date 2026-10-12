#include "ArcaneCombatData.h"


const UArcaneCombatData* UArcaneCombatData::Singleton = nullptr;

const UArcaneCombatData* UArcaneCombatData::Get()
{
	// IsValid 而非 nullptr 判空：防历史包里已 GC 的悬垂指针再次触发野读崩溃
	// （当前包已由 GameInstance::LoadedCombatData 硬引用根住，这里是第二道保险）
	if (Singleton == nullptr || !IsValid(Singleton))
	{
		ensureAlwaysMsgf(false,
			TEXT("UArcaneCombatData::Get() 调用时注册表未初始化！"
				 "请在 GameInstance::Init 中调用 SetRegistry(CombatDataAsset)。"));
		return nullptr;
	}
	
	return Singleton;
}


