// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneProjectile.h"

#include "AbilitySystemInterface.h"
#include "ArcaneCombatant.h"
#include "ArcaneCombatData.h"
#include "ArcaneCombatDataLibrary.h"
#include "ArcaneCombatFunctionLibrary.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/ProjectileMovementComponent.h"

// 与锁定裁决口同一套"打胸部"的约定：GetActorLocation 是胶囊中心，打脚底命中率偏低
static constexpr float ArcaneBoltAimZ = 60.0f;

// Sets default values
AArcaneProjectile::AArcaneProjectile()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComp"));
	RootComponent = CollisionComp;
	CollisionComp->SetSphereRadius(30.0f); // 运行时被 AbilityTable 的 Radius 覆盖
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionComp->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComp->SetCollisionResponseToAllChannels(ECR_Block);
	CollisionComp->SetNotifyRigidBodyCollision(true);
	CollisionComp->SetGenerateOverlapEvents(false);
	
	MovementComp = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("MovementComp"));
	// UpdatedComponent 由 UMovementComponent::InitializeComponent 兜到 Owner 的 RootComponent，这里不手工绑
	MovementComp->bSweepCollision = true;              // 高速弹靠扫掠防穿模
	MovementComp->bRotationFollowsVelocity = true;
	MovementComp->bShouldBounce = false;
	MovementComp->ProjectileGravityScale = 0.0f; // 弹道走"跟踪"而非抛物线，重力关掉 
	
	// 服务器模拟 + 位移复制：客户端拿到位置快照（NetUpdateFrequency 用引擎默认 100Hz，高于帧率，够用）
	bReplicates = true;
	AActor::SetReplicateMovement(true);
	bNetUseOwnerRelevancy = true;  // 跟着发射者的相关性走：弹飞远了队友/对手仍能看到
}

void AArcaneProjectile::InitShot(AActor* InShooter, AActor* InTarget, FName InAbilityID,
                                 const FVector& InInitialDirection, float InMaxTravelDistance, const FArcaneAbilityRow& Row, 
                                 const FVector& InHomingOffset)
{
	Shooter = InShooter;
	HomingTarget = InTarget;
	AbilityID = InAbilityID;
	HomingOffset = InHomingOffset;
	
	// InitShot 传进来的是世界方向。MovementComp 默认 bInitialVelocityInLocalSpace=true，
	// FinishSpawning 的 InitializeComponent 会把它当成局部方向，再按 SpawnTransform 的旋转转一次。
	// 有目标时 Homing 会把方向纠回来；无目标时没有校正，就会沿“二次旋转”后的方向飞。
	MovementComp->bInitialVelocityInLocalSpace = false;
	
	CollisionComp->SetSphereRadius(Row.Radius);
	MovementComp->InitialSpeed = Row.ProjectileSpeed;
	MovementComp->MaxSpeed = Row.ProjectileSpeed;
	MovementComp->Velocity = InInitialDirection * Row.ProjectileSpeed;
	Speed = Row.ProjectileSpeed;
	TurnRateDeg = Row.HomingTurnRate;
	MaxTravelDistance = InMaxTravelDistance;
	
	// 兜底自毁：数值配错（弹速 0）时不能留下永不消失的弹体；正常情况由 MaxTravelDistance 先触发
	SetLifeSpan(MaxTravelDistance / FMath::Max(Speed, 1.0f) + 1.0f);
}

void AArcaneProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	if (bFinished || OtherActor == nullptr || OtherActor == GetOwner())
	{
		return;
	}
	
	// 友军不挡道：穿过它继续找该打的人（伤害侧还有一遍门禁，这里先保住弹体）
	const int32 SourceTeam = UArcaneCombatFunctionLibrary::GetActorTeamID(GetOwner());
	const int32 TargetTeam = UArcaneCombatFunctionLibrary::GetActorTeamID(OtherActor);
	if (TargetTeam >= 0 && SourceTeam == TargetTeam)
	{
		return;
	}
	
	bFinished = true;
	// 地面/墙没有 ASC：只让弹体消失。空技能的目标点通常就在地面上，
	// 不拦会让每次落地都白跑一次伤害管线并打 Warning。
	if (Cast<IAbilitySystemInterface>(OtherActor) != nullptr)
	{
		ApplyHitOnServer(OtherActor, Hit);
	}
	Destroy();
}

void AArcaneProjectile::ApplyHitOnServer(AActor* Target, const FHitResult& Hit)
{
	AActor* Source = Shooter.Get();
	bool bFound = false;
	const FArcaneAbilityRow Row = UArcaneCombatDataLibrary::GetAbilityRow(AbilityID, bFound);
	if (Source == nullptr || bFound == false)
	{
		ensureAlwaysMsgf(false, TEXT("[Projectile] 命中但数据缺失：AbilityID=%s Shooter=%s"),
			*AbilityID.ToString(), *GetNameSafe(Source));
		
		return;
	}
	
	// 衰减曲线语义与近战一致：按"出膛点 → 命中点"直线距离取样（行名留空即 1.0 倍率）
	const float Distance = FVector::Dist(FireOrigin, Hit.ImpactPoint);
	const float FinalDamage = Row.BaseDamage * UArcaneCombatDataLibrary::SampleFalloffCurve(Row.DamageFalloffCurve, Distance);
	
	// Hit 一路传到底：GE 的 Context.AddHitResult → GameplayCue 的 ImpactPoint/Normal 全靠它（Docs/08 那个坑）
	UArcaneCombatFunctionLibrary::ApplyArcaneDamage(Source, Target, Row.DamageEffectClass, FinalDamage, Hit);
	
	const FArcaneImpactRow Impact = UArcaneCombatDataLibrary::GetImpactRow(Row.ImpactProfileID, bFound);
	if (bFound == false)
	{
		return; // 伤害已结算，冲击配置缺失只影响击退/状态，不该吞掉伤害 
	}
	
	const FVector Impulse = UArcaneCombatFunctionLibrary::BuildDisplacementImpulse(Impact, FireOrigin, Target->GetActorLocation(), Hit.ImpactNormal);
	IArcaneCombatant* Combatant = Cast<IArcaneCombatant>(Target);
	if (Combatant != nullptr && !Impulse.IsNearlyZero())
	{
		Combatant->ApplyCombatKnockback(Impulse);
	}
	
	UArcaneCombatFunctionLibrary::ApplyImpact(Source, Target, StatusEffectClass, Impact, Hit);
}

FVector AArcaneProjectile::SteerToward(const FVector& CurrentDir, const FVector& DesiredDir, float DeltaTime) const
{
	const float MaxStepRad = FMath::DegreesToRadians(TurnRateDeg) * DeltaTime;
	if (FVector::DotProduct(CurrentDir, DesiredDir) >= FMath::Cos(MaxStepRad))
	{
		return DesiredDir;   // 一步之内就能对齐，直接咬住目标
	}
	
	FVector Axis = FVector::CrossProduct(CurrentDir, DesiredDir);
	if (Axis.IsNearlyZero())
	{
		// 完全反向：叉乘退化成长度 0，绕自身右侧翻过去，保证转向不会卡死
		Axis = FVector::CrossProduct(CurrentDir, FVector::RightVector);
	}
	
	return FQuat(Axis.GetSafeNormal(), MaxStepRad) * CurrentDir;
}

// Called when the game starts or when spawned
void AArcaneProjectile::BeginPlay()
{
	Super::BeginPlay();
	
	if (!HasAuthority())
	{
		// 客户端既不模拟弹道也不判命中：关掉本地移动与碰撞，只跟服务器复制的位移走。
		// 否则客户端会自己算一套物理、和服务器判定打架（还会把墙前的一发本地炸掉）
		MovementComp->Deactivate();
		CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SetActorTickEnabled(false);
		
		return;
	}
	
	FireOrigin = GetActorLocation();
	CollisionComp->OnComponentHit.AddDynamic(this, &AArcaneProjectile::OnHit);
	if (Shooter.IsValid())
	{
		CollisionComp->IgnoreActorWhenMoving(Shooter.Get(), true);
	}
}

// Called every frame
void AArcaneProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	if (bFinished)
	{
		return;
	}
	
	TraveledDistance += MovementComp->Velocity.Size() * DeltaTime;
	if (TraveledDistance > MaxTravelDistance)
	{
		bFinished = true;
		Destroy();
		
		return;
	}
	
	// 目标中途死亡/被销毁 → 不再修正方向，按现有方向飞到射程尽头
	const AActor* Target = HomingTarget.Get();
	if (Target == nullptr || TurnRateDeg <= 0.0f)
	{
		return;
	}
	
	const FVector CurrentDir = MovementComp->Velocity.GetSafeNormal();
	const FVector DesiredDir = (Target->GetActorLocation() + FVector(0.0f, 0.0f, ArcaneBoltAimZ) + HomingOffset - GetActorLocation()).GetSafeNormal();
	
	MovementComp->Velocity = SteerToward(CurrentDir, DesiredDir, DeltaTime) * Speed;
}

