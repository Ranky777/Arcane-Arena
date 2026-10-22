#include "ArcaneBotCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Framework/ArcaneGameInstance.h"
#include "GameFramework/SpringArmComponent.h"

AArcaneBotCharacter::AArcaneBotCharacter()
{
	SpectateBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpectateBoom"));
	SpectateBoom->SetupAttachment(Cast<USceneComponent>(GetCapsuleComponent()));
	SpectateBoom->TargetArmLength = 1000.0f;
	SpectateBoom->SetRelativeLocation(FVector(0.0f, 0.0f, 70.0f));
	SpectateBoom->TargetOffset = FVector(0.0f, 40.0f, 0.0f);  // 玩家 Boom 的侧偏；世界空间施加，不随胶囊转
	SpectateBoom->SetRelativeRotation(FRotator(-65.0f, 0.0f, 0.0f));
	SpectateBoom->bUsePawnControlRotation = false; // 固定俯角，跟身体转而不跟瞄准
	SpectateBoom->bEnableCameraLag = true;
	SpectateBoom->CameraLagSpeed = 10.0f;
	SpectateBoom->bDoCollisionTest = true;
 
	SpectateCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("SpectateCamera"));
	SpectateCamera->SetupAttachment(SpectateBoom);
	SpectateCamera->bUsePawnControlRotation = false;
	SpectateCamera->FieldOfView = 90.0f;
}

void AArcaneBotCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	if (const UArcaneGameInstance* GI = GetGameInstance<UArcaneGameInstance>())
	{
		SpectateCamera->SetFieldOfView(GI->GetPlayerFOV());  // FOV 与玩家同源，别猜 90
	}
}

void AArcaneBotCharacter::PostInitializeComponents()
{
	AutoPossessAI = EAutoPossessAI::Disabled;
	
	Super::PostInitializeComponents();
}
