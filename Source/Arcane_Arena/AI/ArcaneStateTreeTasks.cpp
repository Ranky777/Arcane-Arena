#include "AI/ArcaneStateTreeTasks.h"

#include "AI/ArcaneAIController.h"
#include "Arcane_Arena.h"
#include "StateTreeExecutionContext.h"

// EStateTreeRunStatus FArcaneTaskTryActivateAbility::EnterState(FStateTreeExecutionContext& Context,
//                                                               const FStateTreeTransitionResult& Transition) const
// {
// 	FInstanceDataType& Data = Context.GetInstanceData(*this);
// 	
// 	AArcaneAIController* Controller = Cast<AArcaneAIController>(Context.GetOwner());
// 	if (Controller == nullptr)
// 	{
// 		return EStateTreeRunStatus::Failed;
// 	}
// 	
// 	return Controller->TryActivateBotAbilityClass(Data.Ability) ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
// }

EStateTreeRunStatus FArcaneTaskBotAttack::EnterState(FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	AArcaneAIController* Controller = Cast<AArcaneAIController>(Context.GetOwner());
	if (Controller == nullptr)
	{
		return EStateTreeRunStatus::Failed;
	}
	if (Data.InputID == EArcaneAbilityInputID::None)
	{
		UE_LOG(LogArcane, Warning, TEXT("[BotAttack] %s: 状态未配置 InputID，放弃"), *Controller->GetName());
		return EStateTreeRunStatus::Failed;
	}
	if (!Controller->FaceEnemyTarget())
	{
		UE_LOG(LogArcane, Warning, TEXT("[BotAttack] %s: 转向失败（无可朝向目标），放弃"), *Controller->GetName());
		return EStateTreeRunStatus::Failed;
	}
	
	if (!Controller->SimulateAbilityInput(Data.InputID, true))
	{
		// 具体原因由 SimulateAbilityInput 自己报警；这里只补一句是哪个状态触发的
		UE_LOG(LogArcane, Warning, TEXT("[BotAttack] %s: 按下 InputID=%d 未被 ASC 接受"), *Controller->GetName(), static_cast<int32>(Data.InputID));
		return EStateTreeRunStatus::Failed;
	}
	
	Data.bPressed = true;
	Data.Elapsed = 0.0f;
	
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FArcaneTaskBotAttack::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	AArcaneAIController* Controller = Cast<AArcaneAIController>(Context.GetOwner());
	
	Data.Elapsed += DeltaTime;
	if (Data.Elapsed >= Data.HoldTime)
	{
		if (Controller != nullptr)
		{
			Controller->SimulateAbilityInput(Data.InputID, false);
		}
		
		Data.bPressed = false;
		
		return EStateTreeRunStatus::Succeeded;
	}
	
	return EStateTreeRunStatus::Running;
}

void FArcaneTaskBotAttack::ExitState(FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	// 安全网：状态被打断时保证松开，防止 GA 卡在 WaitInputRelease
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (Data.bPressed)
	{
		if (AArcaneAIController* Controller = Cast<AArcaneAIController>(Context.GetOwner()))
		{
			Controller->SimulateAbilityInput(Data.InputID, false);
		}
		
		Data.bPressed = false;
	}
}

EStateTreeRunStatus FArcaneTaskBotDash::EnterState(FStateTreeExecutionContext& Context,
                                                   const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	
	AArcaneAIController* Controller = Cast<AArcaneAIController>(Context.GetOwner());
	if (Controller == nullptr)
	{
		return EStateTreeRunStatus::Failed;
	}
	
	return Controller->TryBotDash(Data.bDashTowardTarget) ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FArcaneTaskBotFace::EnterState(FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	AArcaneAIController* Controller = Cast<AArcaneAIController>(Context.GetOwner());
	if (Controller == nullptr)
	{
		return EStateTreeRunStatus::Failed;
	}
	
	Controller->TrackEnemyFacing(true);
	
	// SetFocus 是"安装一个每帧自动重算的持续效果"的一次性指令，任务本身没有过程 ⇒ 立即完成。
	return EStateTreeRunStatus::Succeeded;
}

void FArcaneTaskBotFace::ExitState(FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	if (AArcaneAIController* Controller = Cast<AArcaneAIController>(Context.GetOwner()))
	{
		Controller->TrackEnemyFacing(false);
	}
}
