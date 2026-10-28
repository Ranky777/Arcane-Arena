// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneAIController.h"

#include "Arcane_Arena.h"
#include "EngineUtils.h"
#include "Character/ArcaneBotCharacter.h"
#include "Combat/ArcaneCombatData.h"
#include "Combat/ArcaneCombatDataLibrary.h"
#include "Combat/ArcaneCombatFunctionLibrary.h"
#include "Components/StateTreeAIComponent.h"
#include "Core/ArcaneTeamUtils.h"
#include "Framework/ArcaneGameState.h"
#include "GAS/ArcaneAbilitySystemComponent.h"
#include "GAS/ArcaneAttributeSet.h"
#include "GAS/ArcaneGameplayAbility.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"


// Sets default values
AArcaneAIController::AArcaneAIController()
{
	// 关键开关：引擎在 PostInitializeComponents 里调 InitPlayerState()，
	// 用 GameMode.PlayerStateClass 给 Bot 建真 PlayerState（仅服务器/单机执行）
	bWantsPlayerState = true;
	
	PrimaryActorTick.bCanEverTick = true;
	
	Perception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("Perception"));
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	BotBrain = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("BotBrain"));
	// 我们的生成顺序是"控制器在前、Pawn 在后"：BeginPlay 自动启动会因 Pawn 上下文
	// 缺失而永久失败（SetContextData: Could not find context actor of type Pawn）。
	// 关闭自动启动，改由 OnPossess 驱动——回合重生（UnPossess→Possess）也随之天然重启。
	BotBrain->SetStartLogicAutomatically(false);
}

AArcaneBotCharacter* AArcaneAIController::GetBotCharacter() const
{
	return Cast<AArcaneBotCharacter>(GetPawn());
}

int32 AArcaneAIController::GetArcaneTeamID() const
{
	return GetArcanePlayerState() ? GetArcanePlayerState()->TeamID : INDEX_NONE;
}

void AArcaneAIController::DestroyBotCompletely()
{
	if (APawn* BotPawn = GetPawn())
	{
		UnPossess();
		BotPawn->Destroy();
	}
	
	CleanupPlayerState();
	Destroy();
}

AActor* AArcaneAIController::ResolveEnemyTarget() const
{
	const AArcanePlayerState* MyPS = GetArcanePlayerState();
	if (MyPS == nullptr || MyPS->bIsDead)
	{
		return nullptr;
	}
	
	const AArcaneGameState* GS = GetWorld() ? GetWorld()->GetGameState<AArcaneGameState>() : nullptr;
	if (GS == nullptr || GS->MatchPhase != EArcaneMatchPhase::Playing)
	{
		return nullptr;
	}
	
	const APawn* SelfPawn = GetPawn();
	if (SelfPawn == nullptr)
	{
		return nullptr;
	}
	
	AActor* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	const FVector SelfLocation = SelfPawn->GetActorLocation();
	
	// 首选当前视觉里最近的敌人
	TArray<AActor*> Perceived;
	if (Perception != nullptr)
	{
		Perception->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), Perceived);
	}
	
	for (AActor* Candidate : Perceived)
	{
		if (IsHostileCharacter(Candidate) == false)
		{
			continue;
		}
		
		const float DistSq = FVector::DistSquared(Candidate->GetActorLocation(), SelfLocation);
		if (DistSq < BestDistSq)
		{
			Best = Candidate;
			BestDistSq = DistSq;
		}
	}
	
	// 兜底听觉，交战半径内最近的敌对成员，防止Bot在掩体后面壁罚站
	if (Best == nullptr)
	{
		const float EngagementRadiusSq = FMath::Square(EngagementRadius);
		for (TActorIterator<APawn> It(GetWorld()); It; ++It)
		{
			APawn* Other = *It;
			if (IsHostileCharacter(Other) == false)
			{
				continue;
			}
			
			const float DistSq = FVector::DistSquared(Other->GetActorLocation(), SelfLocation);
			if (DistSq < BestDistSq && DistSq < EngagementRadiusSq)
			{
				Best = Other;
				BestDistSq = DistSq;
			}
		}
	}
	
	return Best;
}

bool AArcaneAIController::IsSelfDead() const
{
	const AArcanePlayerState* PS = GetArcanePlayerState();
	
	return PS == nullptr || PS->bIsDead;
}

float AArcaneAIController::GetTargetDistance() const
{
	const AActor* Target = ResolveEnemyTarget();
	const APawn* SelfPawn = GetPawn();
	
	if (Target == nullptr || SelfPawn == nullptr)
	{
		return TNumericLimits<float>::Max();
	}
	
	return FVector::Dist2D(SelfPawn->GetActorLocation(), Target->GetActorLocation());
}

bool AArcaneAIController::IsHostileCharacter(AActor* Actor) const
{
	if (Actor == nullptr || Actor == GetPawn())
	{
		return false;
	}
	
	const IArcaneCombatant* Combatant = Cast<IArcaneCombatant>(Actor);
	if (Combatant == nullptr || !Combatant->IsCombatAlive())
	{
		return false;
	}
	
	const int32 MyTeam = GetArcaneTeamID();
	const int32 OtherTeam = ArcaneTeam::GetTeamId(Actor);
 
	return MyTeam >= 0 && OtherTeam >= 0 && MyTeam != OtherTeam;
}

float AArcaneAIController::GetTargetHealthFraction() const
{
	AActor* Target = ResolveEnemyTarget();
	const IAbilitySystemInterface* TargetASI = Cast<IAbilitySystemInterface>(Target);
	
	if (TargetASI != nullptr)
	{
		const UAbilitySystemComponent* TargetASC = TargetASI->GetAbilitySystemComponent();
		if (TargetASC != nullptr)
		{
			const float MaxH = TargetASC->GetNumericAttribute(
				UArcaneAttributeSet::GetMaxHealthAttribute());
 
			if (MaxH > 0.0f)
			{
				return FMath::Clamp(
					TargetASC->GetNumericAttribute(
						UArcaneAttributeSet::GetHealthAttribute()) / MaxH,
					0.0f, 1.0f);
			}
		}
	}
	
	return 1.0f;
}

bool AArcaneAIController::IsAbilityReady(TSubclassOf<UGameplayAbility> AbilityClass) const
{
	return HasAbilitySpec(AbilityClass) && GetAbilityCooldownRemaining(AbilityClass) <= 0.0 && HasEnoughEnergyForAbility(AbilityClass);
}

bool AArcaneAIController::HasEnoughEnergyForAbility(TSubclassOf<UGameplayAbility> AbilityClass) const
{
	const UArcaneGameplayAbility* AbilityCDO = AbilityClass ? Cast<UArcaneGameplayAbility>(AbilityClass.GetDefaultObject()) : nullptr;
	if (AbilityCDO == nullptr || AbilityCDO->AbilityID.IsNone())
	{
		return false;
	}
	
	bool bFound = false;
	const FArcaneAbilityRow Row = UArcaneCombatDataLibrary::GetAbilityRow(AbilityCDO->AbilityID, bFound);
	if (!bFound)
	{
		return false;
	}
	
	const UArcaneAbilitySystemComponent* ASC = GetBotASC();
	if (ASC == nullptr)
	{
		return false;
	}
	
	return ASC->GetNumericAttribute(UArcaneAttributeSet::GetEnergyAttribute()) >= Row.EnergyCost;
}

bool AArcaneAIController::HasAbilitySpec(TSubclassOf<UGameplayAbility> AbilityClass) const
{
	if (AbilityClass == nullptr)
	{
		return false;
	}
	
	const UArcaneAbilitySystemComponent* ASC = GetBotASC();
	
	return ASC != nullptr && ASC->FindAbilitySpecFromClass(AbilityClass) != nullptr;
}

float AArcaneAIController::GetAbilityCooldownRemaining(TSubclassOf<UGameplayAbility> AbilityClass) const
{
	UArcaneAbilitySystemComponent* ASC = GetBotASC();
	const UArcaneGameplayAbility* AbilityCDO = AbilityClass ? Cast<UArcaneGameplayAbility>(AbilityClass.GetDefaultObject()) : nullptr;
	if (ASC == nullptr || AbilityCDO == nullptr || AbilityCDO->InputID == EArcaneAbilityInputID::None)
	{
		// 无输入槽 = 没有冷却语义可查，返回 0 放行，交给激活侧兜底
		return 0.0f;
	}
	
	float Remaining = 0.0f;
	float Duration = 0.0f;
	ASC->GetCooldownTimes(AbilityCDO->InputID, Remaining, Duration);
	
	return Remaining;
}

bool AArcaneAIController::IsTargetInAbilityRange(TSubclassOf<UGameplayAbility> AbilityClass) const
{
	const UArcaneGameplayAbility* AbilityCDO = AbilityClass ? Cast<UArcaneGameplayAbility>(AbilityClass.GetDefaultObject()) : nullptr;
	if (AbilityCDO == nullptr || AbilityCDO->AbilityID.IsNone())
	{
		return false;
	}
	
	bool bFound = false;
	const FArcaneAbilityRow Row = UArcaneCombatDataLibrary::GetAbilityRow(AbilityCDO->AbilityID, bFound);
	
	return bFound && GetTargetDistance() <= Row.Range;
}

bool AArcaneAIController::HasLineOfSightToTarget() const
{
	AActor* Target = ResolveEnemyTarget();
	
	// 复用服务器裁决同一条规则：敌我/存活/视线，避免 AI 和服务器各写一套
	return Target != nullptr && UArcaneCombatFunctionLibrary::IsLockableTarget(GetPawn(), Target);
}

FVector AArcaneAIController::RollHomingAimError() const
{
	if (HomingAimErrorRadius <= 0.0f)
	{
		return FVector::ZeroVector;
	}
	
	// 只做水平面误差：竖直误差容易让弹体主动扎地，看起来更像 Bug 而不是枪法差
	const FVector2D Disc = FMath::RandPointInCircle(HomingAimErrorRadius);
	
	return FVector(Disc.X, Disc.Y, 0.0f);
}

bool AArcaneAIController::IsEnemyChargingThreat() const
{
	if (ChargingThreatTag.IsValid() == false)
	{
		return false;
	}
	
	AActor* Target = ResolveEnemyTarget();
	const IAbilitySystemInterface* TargetASI = Cast<IAbilitySystemInterface>(Target);
	if (TargetASI != nullptr)
	{
		if (const UAbilitySystemComponent* TargetASC = TargetASI->GetAbilitySystemComponent())
		{
			return TargetASC->HasMatchingGameplayTag(ChargingThreatTag);
		}
	}
	
	return false;
}

bool AArcaneAIController::IsInAttackRange() const
{
	return GetTargetDistance() <= AttackRange;
}

UArcaneAbilitySystemComponent* AArcaneAIController::GetBotASC() const
{
	const AArcanePlayerState* PS = GetArcanePlayerState();
	
	return PS ? PS->GetArcaneASC() : nullptr;
}

void AArcaneAIController::TrackEnemyFacing(bool bTrack)
{
	const AActor* Target = bTrack ? ResolveEnemyTarget() : nullptr;
	
	// focus 到自身 = 无外部焦点，UpdateControlRotation 会退回沿用 Pawn 当前朝向（冲刺背向时用）
	SetFocus(Target != nullptr ? const_cast<AActor*>(Target) : static_cast<AActor*>(GetPawn()), EAIFocusPriority::Gameplay);
}

bool AArcaneAIController::SimulateAbilityInput(EArcaneAbilityInputID InputID, bool bPressed)
{
	UArcaneAbilitySystemComponent* ASC = GetBotASC();
	if (ASC == nullptr)
	{
		UE_LOG(LogArcane, Warning, TEXT("[BotInput] %s: Bot ASC 为空（PlayerState 未就绪？）"), *GetName());
		return false;
	}
	
	if (bPressed)
	{
		// AbilityLocalInputPressed 靠 Spec.InputID 匹配；这里匹配不到就是"按了个没人听的键"，GAS 会静默吞掉
		if (ASC->FindAbilitySpecFromInputID(static_cast<int32>(InputID)) == nullptr)
		{
			UE_LOG(LogArcane, Warning, TEXT("[BotInput] %s: 输入槽 %d 没有绑定的能力 Spec（未授予或 Spec.InputID 不匹配）"),
				*GetName(), static_cast<int32>(InputID));
			return false;
		}
		
		UE_LOG(LogArcane, Log, TEXT("[BotInput] %s: 按下输入槽 %d"), *GetName(), static_cast<int32>(InputID));
		ASC->AbilityLocalInputPressed(static_cast<int32>(InputID));
	}
	else
	{
		ASC->AbilityLocalInputReleased(static_cast<int32>(InputID));
	}
	
	return true;
}

bool AArcaneAIController::FaceEnemyTarget(bool bFaceAway)
{
	APawn* SelfPawn = GetPawn();
	const AActor* Target = ResolveEnemyTarget();
	if (SelfPawn == nullptr || Target == nullptr)
	{
		return false;
	}
	
	FVector ToTarget = (Target->GetActorLocation() - SelfPawn->GetActorLocation()).GetSafeNormal2D();
	if (ToTarget.IsNearlyZero())
	{
		return false;
	}
	
	const FVector FacingDir = bFaceAway ? -ToTarget : ToTarget;
	if (bFaceAway)
	{
		// 背向冲刺：先让 focus 不再每帧把朝向拽回目标
		TrackEnemyFacing(false);
	}
	
	SelfPawn->SetActorRotation(FacingDir.Rotation());
	SetControlRotation(FacingDir.Rotation());
	
	return true;
}
//
// bool AArcaneAIController::TryActivateBotAbilityClass(TSubclassOf<UGameplayAbility> AbilityClass)
// {
// 	UArcaneAbilitySystemComponent* ASC = GetBotASC();
// 	if (ASC == nullptr || AbilityClass == nullptr)
// 	{
// 		return false;
// 	}
// 	
// 	FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(AbilityClass);
// 	
// 	return Spec != nullptr && ASC->TryActivateAbility(Spec->Handle);
// }

bool AArcaneAIController::TryBotDash(bool bTowardTarget)
{
	if (DashAbilityClass == nullptr)
	{
		UE_LOG(LogArcane, Warning, TEXT("[BotDash] %s: DashAbilityClass 未配置"), *GetName());
		return false;
	}
	
	if (!FaceEnemyTarget(!bTowardTarget))
	{
		UE_LOG(LogArcane, Warning, TEXT("[BotDash] %s: 转向失败（无可朝向目标）"), *GetName());
		return false;
	}
	
	// Dash 走输入通道：点按（按下即松）
	const UArcaneGameplayAbility* DashCD0 = Cast<UArcaneGameplayAbility>(DashAbilityClass.GetDefaultObject());
	if (DashCD0 == nullptr || DashCD0->InputID == EArcaneAbilityInputID::None)
	{
		UE_LOG(LogArcane, Warning, TEXT("[BotDash] %s: Dash 能力没有输入槽（GA_Dash 的 InputID=None？）"), *GetName());
		return false;
	}
	
	if (!SimulateAbilityInput(DashCD0->InputID, true))
	{
		return false;
	}
	SimulateAbilityInput(DashCD0->InputID, false);
	
	return true;
}

void AArcaneAIController::BeginPlay()
{
	Super::BeginPlay();
	
	if (Perception == nullptr || SightConfig == nullptr)
	{
		return;
	}
	
	SightConfig->SightRadius = 1200.0f;
	SightConfig->LoseSightRadius = 1800.0f;
	SightConfig->PeripheralVisionAngleDegrees = 360.0f;
	SightConfig->SetMaxAge(5.0f);
	
	Perception->ConfigureSense(*SightConfig);
	Perception->SetDominantSense(UAISense_Sight::StaticClass());
}

void AArcaneAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	
	// Super 里已跑过 Pawn 的 PossessedBy → ASC 的 InitAbilityActorInfo/GiveAbility 完成，此时挂探针最稳
	if (UArcaneAbilitySystemComponent* ASC = GetBotASC())
	{
		BotAbilityFailedHandle = ASC->AbilityFailedCallbacks.AddUObject(this, &AArcaneAIController::OnBotAbilityFailed);
	}
	
	if (BotBrain != nullptr)
	{
		BotBrain->StartLogic();
	}
}

void AArcaneAIController::OnBotAbilityFailed(const UGameplayAbility* Ability, const FGameplayTagContainer& FailureTags)
{
	UE_LOG(LogArcane, Warning, TEXT("[BotFail] %s: %s 激活被 GAS 拒绝，原因标签=[%s]"),
		*GetName(), *GetNameSafe(Ability), *FailureTags.ToStringSimple());
}

void AArcaneAIController::OnUnPossess()
{
	if (UArcaneAbilitySystemComponent* ASC = GetBotASC())
	{
		ASC->AbilityFailedCallbacks.Remove(BotAbilityFailedHandle);
	}
	BotAbilityFailedHandle.Reset();
	
	if (BotBrain != nullptr)
	{
		BotBrain->StopLogic(TEXT("UnPossessed"));
	}
	
	Super::OnUnPossess();
}
