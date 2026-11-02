// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneShieldCueNotify.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Arcane_Arena.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"


static AActor* ResolveCueAttachTarget(AActor* CueTarget)
{
	if (CueTarget == nullptr)
	{
		return nullptr;
	}

	// ASC 挂在 PlayerState 上：Cue 的 MyTarget 是 PS，不是角色
	if (const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(CueTarget))
	{
		if (UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
		{
			if (AActor* Avatar = ASC->GetAvatarActor())
			{
				return Avatar;
			}
		}
	}

	// 兜底：PS 直接拿当前 Pawn
	if (const APlayerState* PS = Cast<APlayerState>(CueTarget))
	{
		if (APawn* Pawn = PS->GetPawn())
		{
			return Pawn;
		}
	}

	return CueTarget;
}


// Sets default values
AArcaneShieldCueNotify::AArcaneShieldCueNotify()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	
	ShieldMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShieldMesh"));
	RootComponent = ShieldMesh;
	
	ShieldMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ShieldMesh->SetCastShadow(false);
	ShieldMesh->SetReceivesDecals(false);
	ShieldMesh->SetGenerateOverlapEvents(false);
	ShieldMesh->SetMobility(EComponentMobility::Movable);
	ShieldMesh->SetTranslucentSortPriority(10);
	ShieldMesh->SetVisibility(false);
	
	// 由 OnRemove 启动淡出，淡出结束后调用 K2_EndGameplayCue 归还/回收；不能让基类在 OnRemove 后立即回收
	bAutoDestroyOnRemove = false;
	bAutoAttachToOwner = false;
}

bool AArcaneShieldCueNotify::OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	// 基类 OnActive 是空实现、固定 return false：它表示“基类未处理”，不是失败，
	// 不能拿它的返回值做前置条件，否则整个 Cue 都不会执行。
	if (MyTarget == nullptr)
	{
		return false;
	}

	AActor* AttachTarget = ResolveCueAttachTarget(MyTarget);
	if (AttachTarget == nullptr)
	{
		return false;
	}

	if (ShieldMeshAsset != nullptr)
	{
		ShieldMesh->SetStaticMesh(ShieldMeshAsset);
	}
	else if (ShieldMesh->GetStaticMesh() == nullptr)
	{
		UE_LOG(LogArcane, Warning, TEXT("[ShieldCue] %s 未配置 ShieldMeshAsset，护盾球不可见"), *GetName());
	}

	float HalfHeight = 88.0f;
	float CapsuleRadius = 34.0f;
	if (const ACharacter* Character = Cast<ACharacter>(AttachTarget))
	{
		if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
			CapsuleRadius = Capsule->GetScaledCapsuleRadius();
		}
	}

	// 引擎 Sphere 半径 50：球心放在胶囊中心，半径包住胶囊
	const float ShieldRadius = FMath::Max(HalfHeight, CapsuleRadius) * ShieldPadding;
	InitialScale = FVector(ShieldRadius / 50.0f);

	if (USceneComponent* TargetRoot = AttachTarget->GetRootComponent())
	{
		// 直接挂 ShieldMesh，而不是挂 Cue Actor 的 Root：
		// BP 子类可能带 DefaultSceneRoot，Actor Root 与球体组件不是同一个组件，
		// 只挂 Actor 会让球体留在生成点。
		ShieldMesh->AttachToComponent(TargetRoot, FAttachmentTransformRules::KeepRelativeTransform);
		ShieldMesh->SetRelativeLocation(FVector::ZeroVector);
		ShieldMesh->SetRelativeScale3D(InitialScale);
	}
	else
	{
		UE_LOG(LogArcane, Warning, TEXT("[ShieldCue] %s 的 RootComponent 为空，护盾无法跟随"), *GetNameSafe(AttachTarget));
		ShieldMesh->SetRelativeScale3D(InitialScale);
	}

	ShieldMesh->SetVisibility(true);

	// 首帧就给 50% 基础透明度，避免第一次 Tick 之前完全不可见
	FadeAlpha = 0.5f;
	ShieldMID = ShieldMesh->CreateDynamicMaterialInstance(0, ShieldMaterial);
	if (ShieldMID != nullptr)
	{
		ShieldMID->SetVectorParameterValue(TEXT("ShieldColor"), ShieldColor);
		ShieldMID->SetScalarParameterValue(TEXT("Opacity"), BaseOpacity * FadeAlpha);
	}

	bFadingOut = false;
	SetActorTickEnabled(true);
	return true;
}

bool AArcaneShieldCueNotify::WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	// 远端复制版 GE 可能只走 WhileActive（中途加入）；
	// 同一台机器上也可能紧跟 OnActive 再派发一次，已激活就不重复初始化。
	return IsActorTickEnabled() ? true : OnActive(MyTarget, Parameters);
}

bool AArcaneShieldCueNotify::OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	// 不能调基类：基类 OnRemove 会 SetActorHiddenInGame(true)，淡出还没开始就被藏起来；
	// 也不能用它的返回值做失败判据（基类固定返回 false）。
	bFadingOut = true;
	// 兜底：从未激活过的 Cue Actor（例如只收到 WhileActive 却没有可挂载目标）也让 Tick 走一次收尾，
	// 避免在 OnRemove 内同步回收造成重入
	SetActorTickEnabled(true);
	return true;
}

// Called every frame
void AArcaneShieldCueNotify::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	const float TargetAlpha = bFadingOut ? 0.0f : 1.0f;
	const float InterpSpeed = bFadingOut
		? (1.0f / FMath::Max(0.01f, FadeOutTime))
		: (1.0f / FMath::Max(0.01f, FadeInTime));
 
	FadeAlpha = FMath::FInterpTo(FadeAlpha, TargetAlpha, DeltaTime, InterpSpeed);
 
	if (ShieldMID != nullptr)
	{
		const float Pulse = 1.0f + FMath::Sin(GetWorld()->GetTimeSeconds() * PulseSpeed) * PulseAmplitude;
		ShieldMID->SetScalarParameterValue(TEXT("Opacity"), BaseOpacity * FadeAlpha * Pulse);
	}
 
	// 破裂时轻微扩张；淡出结束归还 CueActor
	const float ScaleMul = bFadingOut ? FMath::Lerp(1.12f, 1.0f, FadeAlpha) : 1.0f;
	ShieldMesh->SetRelativeScale3D(InitialScale * ScaleMul);
 
	if (bFadingOut && FadeAlpha <= 0.01f)
	{
		// UE5.8 默认开启 CueActor 回收：直接 Destroy 会报 "should not happen" 并绕过对象池
		SetActorTickEnabled(false);
		K2_EndGameplayCue();
	}
}

