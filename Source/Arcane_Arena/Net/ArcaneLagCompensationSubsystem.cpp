// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneLagCompensationSubsystem.h"

#include "Arcane_Arena.h"
#include "EngineUtils.h"
#include "Character/ArcaneCharacter.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerState.h"

static TAutoConsoleVariable<int32> CVarLagCompEnabled(
	TEXT("Arcane.LagComp.Enable"), 1,
	TEXT("近战服务器回退（1=开，0=关）。"), ECVF_Default);
 
static TAutoConsoleVariable<int32> CVarLagCompDebug(
	TEXT("Arcane.LagComp.Debug"), 0,
	TEXT("绘制历史胶囊与回退扫掠（1=开）。"), ECVF_Default);
 
static TAutoConsoleVariable<float> CVarLagCompMaxRewindMs(
	TEXT("Arcane.LagComp.MaxRewindMs"), 250.0f,
	TEXT("最大回退窗口（毫秒）。"), ECVF_Default);
 
static TAutoConsoleVariable<float> CVarLagCompInterpMs(
	TEXT("Arcane.LagComp.InterpDelayMs"), 100.0f,
	TEXT("客户端插值缓冲估计（毫秒），加到 RTT/2 上。"), ECVF_Default);
 

void UArcaneLagCompensationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	Histories.Reset();
	LastRecordTime = 0.0;
}

void UArcaneLagCompensationSubsystem::Deinitialize()
{
	Histories.Reset();
	Super::Deinitialize();
}

void UArcaneLagCompensationSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (World == nullptr || World->GetNetMode() == NM_Client)
	{
		return;
	}
	
	const double Now = World->GetTimeSeconds();
	if (Now - LastRecordTime < SampleInterval)
	{
		return;
	}
	
	LastRecordTime = Now;
	
	for (TActorIterator<AArcaneCharacter> It(World); It; ++It)
	{
		RecordSnapshot(*It, Now);
	}
	
	PruneHistory(Now);
}

TStatId UArcaneLagCompensationSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UArcaneLagCompensationSubsystem, STATGROUP_Tickables);
}

bool UArcaneLagCompensationSubsystem::IsTickable() const
{
	const UWorld* World = GetWorld();
	return World != nullptr	 && World->GetNetMode() != NM_Client;
}

void UArcaneLagCompensationSubsystem::SweepMeleeAgainstHistory(const AArcaneCharacter* Attacker, const FVector& Start,
	const FVector& End, float SweepRadius, float RewindSeconds, TArray<FHitResult>& OutHits) const
{
	OutHits.Reset();
	
	UWorld* World = GetWorld();
	if (Attacker == nullptr || World == nullptr)
	{
		return;
	}
	
	const double Now = World->GetTimeSeconds();
	const double RewindTime = Now - FMath::Clamp(static_cast<double>(RewindSeconds), 0.0, MaxHistorySeconds);
	const bool bDrawDebug = CVarLagCompDebug.GetValueOnGameThread() != 0;
	
	// 回退后的视线检查只让“世界几何”挡住近战，不让人物互相挡。
	// 否则一刀扫到两个人时，后面那个会被前面那个的胶囊挡住，丢失多目标命中。
	TArray<AActor*> IgnoreActors;
	for (TActorIterator<AArcaneCharacter> It(World); It; ++It)
	{
		IgnoreActors.Add(*It);
	}
	
	for (const TPair<AArcaneCharacter*, FArcaneCapsuleHistory>& Pair : Histories)
	{
		AArcaneCharacter* Target = Pair.Key;
		if (!IsValid(Target) || Target == Attacker)
		{
			continue;
		}
		
		const IArcaneCombatant* Combatant = Cast<IArcaneCombatant>(Target);
		if (Combatant == nullptr || !Combatant->IsCombatAlive())
		{
			continue;
		}
		
		UCapsuleComponent* TargetCapsule = Target->GetCapsuleComponent();
		if (TargetCapsule == nullptr || TargetCapsule->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
		{
			continue;
		}
		
		FArcaneCapsuleSnapshot Snapshot;
		if (!SampleHistory(Target, RewindTime, Snapshot))
		{
			continue;
		}
		
		// 胶囊的“内线段”两端：中心 ± Up * (HalfHeight - Radius)。
		const FVector Up = Snapshot.Rotation.GetUpVector();
		const float InnerHalfHeight = FMath::Max(0.0f, Snapshot.HalfHeight - Snapshot.Radius);
		const FVector CapsuleBottom = Snapshot.Location - Up * InnerHalfHeight;
		const FVector CapsuleTop = Snapshot.Location + Up * InnerHalfHeight;
		
		FVector ClosestOnAttack;
		FVector ClosestOnTarget;
		FMath::SegmentDistToSegmentSafe(Start, End, CapsuleBottom, CapsuleTop, ClosestOnAttack, ClosestOnTarget);
		
		const float MaxDistance = FMath::Max(0.0f, SweepRadius) + Snapshot.Radius;
		if (FVector::DistSquared(ClosestOnAttack, ClosestOnTarget) > static_cast<double>(FMath::Square(MaxDistance)))
		{
			continue;
		}
		
		// 回退后的位置也要有视线：避免“回退到墙后”打出穿墙刀。
		FHitResult Blocker;
		
		FCollisionQueryParams Params(FName(TEXT("ArcaneMeleeLagComp")), false, Attacker);
		Params.AddIgnoredActors(IgnoreActors);
		Params.AddIgnoredActor(Target);
 
		if (World->LineTraceSingleByChannel(
			Blocker, ClosestOnAttack, ClosestOnTarget, ECC_Visibility, Params))
		{
			continue;
		}
		 
		FVector ImpactNormal = (ClosestOnAttack - ClosestOnTarget).GetSafeNormal();
		if (ImpactNormal.IsNearlyZero())
		{
			ImpactNormal = -Attacker->GetActorForwardVector();
		}
 
		FHitResult Hit;
		Hit.HitObjectHandle = FActorInstanceHandle(Target);
		Hit.Component = TargetCapsule;
		Hit.bBlockingHit = true;
		Hit.Location = ClosestOnTarget;
		Hit.ImpactPoint = ClosestOnTarget;
		Hit.Normal = ImpactNormal;
		Hit.ImpactNormal = ImpactNormal;
		Hit.TraceStart = Start;
		Hit.TraceEnd = End;
		Hit.Distance = static_cast<float>(FVector::Dist(Start, ClosestOnTarget));
		Hit.Time = 1.0f;
		OutHits.Add(Hit);
		
		if (bDrawDebug)
		{
			// 绿 = 回退后的历史胶囊；青线 = 当前→历史位移；黄线 = 实际判定线段。
			DrawDebugCapsule(World, Snapshot.Location, Snapshot.HalfHeight, Snapshot.Radius,
				Snapshot.Rotation, FColor::Green, false, 1.0f, 0, 1.0f);
			DrawDebugLine(World, Target->GetActorLocation(), Snapshot.Location,
				FColor::Cyan, false, 1.0f, 0, 1.0f);
			DrawDebugLine(World, ClosestOnAttack, ClosestOnTarget,
				FColor::Yellow, false, 1.0f, 0, 2.0f);
		}
	}
	
	UE_LOG(LogArcane, Verbose,
		TEXT("[LagComp] Attacker=%s Ping=%.1fms Rewind=%.1fms Hits=%d"),
		*GetNameSafe(Attacker),
		Attacker->GetPlayerState() ? Attacker->GetPlayerState()->GetPingInMilliseconds() : 0.0f,
		RewindSeconds * 1000.0f,
		OutHits.Num());
}

bool UArcaneLagCompensationSubsystem::HasHistory(const AArcaneCharacter* Character) const
{
	const FArcaneCapsuleHistory* History = Histories.Find(const_cast<AArcaneCharacter*>(Character));
	
	return History != nullptr && History->Samples.Num() > 0;
}

float UArcaneLagCompensationSubsystem::ComputeRewindSeconds(const AArcaneCharacter* Attacker) const
{
	if (Attacker == nullptr || CVarLagCompEnabled.GetValueOnGameThread() == 0)
	{
		return 0.0f;
	}
	
	const APlayerState* PlayerState = Attacker->GetPlayerState();
	const float PingMs = PlayerState ? PlayerState->GetPingInMilliseconds() : 0.0f;
	
	// 主机/Bot：服务器就是本地，不能回退，否则会打“过去的人”。
	if (PingMs < 1.0f)
	{
		return 0.0f;
	}
	
	const float InterpMs = FMath::Max(0.0f, CVarLagCompInterpMs.GetValueOnGameThread());
	const float MaxMs = FMath::Max(0.0f,  CVarLagCompMaxRewindMs.GetValueOnGameThread());
	
	return FMath::Clamp(PingMs * 0.5f + InterpMs, 0.0f, MaxMs) / 1000.0f;
}

void UArcaneLagCompensationSubsystem::RecordSnapshot(AArcaneCharacter* Character, double Now)
{
	if (!IsValid(Character))
	{
		return;
	}
	
	UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	if (Capsule == nullptr)
	{
		return;
	}
	
	FArcaneCapsuleSnapshot Snapshot;
	Snapshot.ServerTime = Now;	
	Snapshot.Location = Character->GetActorLocation(); 
	Snapshot.Rotation = Character->GetActorQuat();
	Snapshot.HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	Snapshot.Radius = Capsule->GetScaledCapsuleRadius();
	
	FArcaneCapsuleHistory& History = Histories.FindOrAdd(Character);
	
	// 传送/重生会让历史不连续；检测到突变就清空，
	// 避免把“重生前的位置”当成可回退的历史。
	if (History.Samples.Num() > 0)
	{
		const FVector LastLocation = History.Samples.Last().Location;
		if (FVector::DistSquared(LastLocation, Snapshot.Location) > FMath::Square(1000.0f))
		{
			History.Samples.Reset();
		}
	}
	
	History.Samples.Add(Snapshot);
}

bool UArcaneLagCompensationSubsystem::SampleHistory(const AArcaneCharacter* Character, double RewindTime,
	FArcaneCapsuleSnapshot& OutSnapshot) const
{
	const FArcaneCapsuleHistory* History = Histories.Find(const_cast<AArcaneCharacter*>(Character));
	if (History == nullptr || History->Samples.Num() == 0)
	{
		return false;
	}
	
	const TArray<FArcaneCapsuleSnapshot>& Samples = History->Samples;
	
	if (RewindTime <= Samples[0].ServerTime)
	{
		OutSnapshot = Samples[0];
		return true;
	}
	
	if (RewindTime >= Samples.Last().ServerTime)
	{
		OutSnapshot = Samples.Last();
		return true;
	}
	
	for (int32 Index = 1; Index < Samples.Num(); Index++)
	{
		if (Samples[Index].ServerTime >= RewindTime)
		{
			const FArcaneCapsuleSnapshot& A = Samples[Index - 1];
			const FArcaneCapsuleSnapshot& B = Samples[Index];
			const double Span = B.ServerTime - A.ServerTime;
			const float Alpha = Span > 1e-6 ? static_cast<float>((RewindTime - A.ServerTime) / Span) : 1.0f;
			
			OutSnapshot.ServerTime = RewindTime;
			OutSnapshot.Location = FMath::Lerp(A.Location, B.Location, Alpha);
			OutSnapshot.Rotation = FQuat::Slerp(A.Rotation, B.Rotation, Alpha);
			OutSnapshot.HalfHeight = FMath::Lerp(A.HalfHeight, B.HalfHeight, Alpha);
			OutSnapshot.Radius = FMath::Lerp(A.Radius, B.Radius, Alpha);
			
			return true;
		}
	}
	
	OutSnapshot = Samples.Last();
	
	return true;
}

void UArcaneLagCompensationSubsystem::PruneHistory(double Now)
{
	const double Cutoff = Now - MaxHistorySeconds;
	
	for (auto It = Histories.CreateIterator(); It; ++It)
	{
		AArcaneCharacter* Character = It.Key();
		if (!IsValid(Character))
		{
			It.RemoveCurrent();
			continue;
		}
		
		FArcaneCapsuleHistory& History = It.Value();
		
		int32 RemoveCount = 0;
		while (RemoveCount < History.Samples.Num() && History.Samples[RemoveCount].ServerTime < Cutoff)
		{
			RemoveCount++;
		}
		
		if (RemoveCount > 0)
		{
			History.Samples.RemoveAt(0, RemoveCount);
		}
		
		if (History.Samples.Num() == 0)
		{
			It.RemoveCurrent();
		}
	}
}
