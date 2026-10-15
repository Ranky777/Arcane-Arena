#include "AI/ArcaneStateTreeEvaluators.h"

#include "AI/ArcaneAIController.h"
#include "Arcane_Arena.h"
#include "StateTreeExecutionContext.h"

// Bot 决策事实快照：定位"状态不切换 / 技能不放"时打开，每 0.5 秒一行
static TAutoConsoleVariable<bool> CVarArcaneBotDebug(
	TEXT("Arcane.AI.BotDebug"),
	false,
	TEXT("每 0.5 秒向 LogArcane 打一行 Bot 决策事实快照（目标/距离/就绪性/冷却）"),
	ECVF_Default);

void FArcaneBotFactsEvaluator::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	
	const AArcaneAIController* Controller = Cast<AArcaneAIController>(Context.GetOwner());
	if (Controller == nullptr)
	{
		Data.TargetActor = nullptr;
		Data.bHasTarget = false;
		Data.bSelfDead = true;
		
		return;
	}
	
	Data.bSelfDead = Controller->IsSelfDead();
	Data.TargetActor = Controller->ResolveEnemyTarget();
	Data.bHasTarget = (Data.TargetActor != nullptr);
	Data.TargetDistance = Controller->GetTargetDistance();
	Data.bInAttackRange = Controller->IsInAttackRange();
	Data.TargetHealthFraction = Controller->GetTargetHealthFraction();
	Data.bEnemyChargingThreat = Controller->IsEnemyChargingThreat();
	Data.bPrimaryReady = Controller->IsAbilityReady(Data.PrimaryAbility);
	Data.bSecondaryReady = Controller->IsAbilityReady(Data.SecondaryAbility);
	Data.bDashReady = Controller->IsAbilityReady(Controller->GetDashAbilityClass());
	
	if (CVarArcaneBotDebug.GetValueOnGameThread())
	{
		Data.DebugSnapshotTimer += DeltaTime;
		if (Data.DebugSnapshotTimer >= 0.5f)
		{
			Data.DebugSnapshotTimer = 0.0f;
			
			UE_LOG(LogArcane, Log,
				TEXT("[BotFacts] %s | target=%s dist=%.0f range=%.0f inRange=%d dead=%d threat=%d hp=%.2f || "
					 "P spec=%d cd=%.2f ready=%d | S spec=%d cd=%.2f ready=%d | D spec=%d cd=%.2f ready=%d"),
				*Controller->GetName(),
				*GetNameSafe(Data.TargetActor),
				Data.TargetDistance,
				Controller->GetAttackRange(),
				Data.bInAttackRange ? 1 : 0,
				Data.bSelfDead ? 1 : 0,
				Data.bEnemyChargingThreat ? 1 : 0,
				Data.TargetHealthFraction,
				Controller->HasAbilitySpec(Data.PrimaryAbility) ? 1 : 0,
				Controller->GetAbilityCooldownRemaining(Data.PrimaryAbility),
				Data.bPrimaryReady ? 1 : 0,
				Controller->HasAbilitySpec(Data.SecondaryAbility) ? 1 : 0,
				Controller->GetAbilityCooldownRemaining(Data.SecondaryAbility),
				Data.bSecondaryReady ? 1 : 0,
				Controller->HasAbilitySpec(Controller->GetDashAbilityClass()) ? 1 : 0,
				Controller->GetAbilityCooldownRemaining(Controller->GetDashAbilityClass()),
				Data.bDashReady ? 1 : 0);
		}
	}
}
