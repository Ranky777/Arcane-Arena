#pragma once
#include "GenericTeamAgentInterface.h"

namespace ArcaneTeam
{
	/**
	 * 队伍归属在 PlayerState 上；Pawn 只是 Avatar。
	 * 这里统一解析 Actor → 队伍，避免各系统 Cast 到具体 Character/PlayerState。
	 */
	inline int32 GetTeamId(const AActor* Actor)
	{
		if (Actor == nullptr)
		{
			return INDEX_NONE;
		}
		
		AActor* MutableActor = const_cast<AActor*>(Actor);
		
		if (IGenericTeamAgentInterface* TeamAgent = Cast<IGenericTeamAgentInterface>(MutableActor))
		{
			const FGenericTeamId TeamId = TeamAgent->GetGenericTeamId();
			if (TeamId.GetId() != FGenericTeamId::NoTeam.GetId())
			{
				return static_cast<int32>(TeamId.GetId());
			}
		}
		
		if (const APawn* Pawn = Cast<APawn>(MutableActor))
		{
			if (IGenericTeamAgentInterface* PS = Cast<IGenericTeamAgentInterface>(Pawn->GetPlayerState()))
			{
				const FGenericTeamId TeamId = PS->GetGenericTeamId();
				if (TeamId.GetId() != FGenericTeamId::NoTeam.GetId())
				{
					return static_cast<int32>(TeamId.GetId());
				}
			}
		}
		
		return INDEX_NONE;
	}
	
	inline ETeamAttitude::Type GetAttitude(int32 TeamA, int32 TeamB)
	{
		if (TeamA < 0 || TeamB < 0)
		{
			return ETeamAttitude::Neutral;
		}
		
		return TeamA == TeamB ? ETeamAttitude::Friendly : ETeamAttitude::Hostile;
	}
}
