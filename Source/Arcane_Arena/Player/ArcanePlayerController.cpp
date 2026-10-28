// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcanePlayerController.h"

#include "ArcaneSpectatorComponent.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Blueprint/UserWidget.h"
#include "Character/ArcaneCharacter.h"
#include "Engine/GameViewportClient.h"
#include "Framework/ArcaneGameMode.h"
#include "GAS/ArcaneAbilitySystemComponent.h"
#include "Player/ArcanePlayerState.h"   // Server RPC 里 GetPlayerState<AArcanePlayerState>() 需要完整类型
#include "UI/ArcaneAbilityRangePreviewWidget.h"
#include "Widgets/SViewport.h"


AArcanePlayerController::AArcanePlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Crosshairs;
	
	SpectatorComponent = CreateDefaultSubobject<UArcaneSpectatorComponent>(TEXT("SpectatorComponent"));
}

void AArcanePlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (DefaultMappingContext)
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}

	ApplyViewportKeyboardFocus();
}

void AArcanePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	BindAbilityInputs();

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (PauseAction != nullptr)
		{
			EIC->BindAction(PauseAction, ETriggerEvent::Started, this, &AArcanePlayerController::TogglePauseMenu);
		}
		
		if (SpectateNextAction != nullptr)
		{
			EIC->BindAction(SpectateNextAction, ETriggerEvent::Started, this, &AArcanePlayerController::OnSpectateNext);
		}
		
		if (SpectatePrevAction != nullptr)
		{
			EIC->BindAction(SpectatePrevAction, ETriggerEvent::Started, this, &AArcanePlayerController::OnSpectatePrev);
		}
	}
}

void AArcanePlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	
	// 换 Pawn/重生：清掉上一个身体残留的范围预览
	HideAbilityRangePreview();
	
	// 重生换 Pawn：清掉观战态，把镜头交回自己新身体。
	// 死亡时我们没 DisableInput，只锁了 Look，这里一并复原。
	if (IsSpectating())
	{
		LeaveSpectatorMode();
	}

	// 死亡表演会 DisableInput（ArcaneCharacter::ApplyDeathPresentation），全项目没有对应的
	// EnableInput；重生走 RestartPlayer → Possess，所以在这里补回，否则死后所有输入绑定永久失效。
	EnableInput(this);

	// 服务器端 Possess 完成后输入映射已就绪；移动绑定留在 BP_CombatCharacter 的
	// 原有逻辑里不动（它已实现 IA_Move/IA_Jump，别重复绑定）

	// 旅行/换 Pawn 后焦点可能仍停在旧的 UMG 上，这里显式抢回键盘焦点
	ApplyViewportKeyboardFocus();
}

void AArcanePlayerController::ApplyViewportKeyboardFocus()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	UGameViewportClient* ViewportClient = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (ViewportClient == nullptr)
	{
		return;
	}

	const TSharedPtr<SViewport> ViewportWidget = ViewportClient->GetGameViewportWidget();
	if (!ViewportWidget.IsValid())
	{
		return;
	}

	// GameAndUI：UMG 依然可点（5.8 的输入模式不再管 hit-test 分组）；
	// SetWidgetToFocus 把键盘焦点定在视口上，按键才会进入 PlayerInput。
	// GameOnly 会永久锁定鼠标、UIOnly 会 SetIgnoreInput(true) 吃掉键盘，两者都会复现本问题。
	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(ViewportWidget.ToSharedRef());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
}

void AArcanePlayerController::TogglePauseMenu()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	if (bPauseMenuOpen)
	{
		ClosePauseMenu();
		return;
	}

	if (PauseMenuWidgetClass == nullptr)
	{
		return;
	}

	PauseMenuWidget = CreateWidget<UUserWidget>(this, PauseMenuWidgetClass);

	if (PauseMenuWidget != nullptr)
	{
		PauseMenuWidget->AddToViewport(100);
		bPauseMenuOpen = true;

		// 只有独立模式才冻结世界：listen server 上 SetPause 会暂停整张地图，
		// 其它玩家会一起卡住；客户端的 SetPause 本来就是 no-op。
		if (GetWorld() && GetWorld()->GetNetMode() == NM_Standalone)
		{
			SetPause(true);
		}

		ApplyViewportKeyboardFocus();
	}
}

void AArcanePlayerController::BindAbilityInputs()
{
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
	{
		for (const FArcaneAbilityInputMapping& Mapping : AbilityInputMappings)
		{
			if (Mapping.InputAction != nullptr && Mapping.AbilityID != EArcaneAbilityInputID::None)
			{
				const int32 InputID = static_cast<int32>(Mapping.AbilityID);
				EIC->BindAction(Mapping.InputAction, ETriggerEvent::Started, this, &AArcanePlayerController::OnAbilityInputPressed, InputID);
				// 释放也要通知 GAS，否则 Spec.InputPressed 永久卡 true，能力结束时会误触发"按住重激活"
				EIC->BindAction(Mapping.InputAction, ETriggerEvent::Completed, this, &AArcanePlayerController::OnAbilityInputReleased, InputID);
			}
		}
	}
}

void AArcanePlayerController::ClosePauseMenu()
{
	if (PauseMenuWidget != nullptr)
	{
		PauseMenuWidget->RemoveFromParent();
		PauseMenuWidget = nullptr;
	}

	bPauseMenuOpen = false;

	if (GetWorld() && GetWorld()->GetNetMode() == NM_Standalone)
	{
		SetPause(false);
	}

	ApplyViewportKeyboardFocus();
}

void AArcanePlayerController::OnAbilityInputPressed(int32 InputID)
{
	if (bPauseMenuOpen)
	{
		return;
	}

	if (const AArcaneCharacter* Avatar = Cast<AArcaneCharacter>(GetPawn()))
	{
		if (Avatar->IsMatchLocked() || IsSpectating())
		{
			return;
		}

		if (UArcaneAbilitySystemComponent* ASC = Avatar->GetArcaneASC())
		{
			ASC->AbilityLocalInputPressed(InputID);
		}
	}
}

void AArcanePlayerController::OnAbilityInputReleased(int32 InputID)
{
	// 与 Pressed 对称，否则菜单开着时松手会把一个没激活过的 InputID 发给 GAS
	if (bPauseMenuOpen)
	{
		return;
	}

	if (const AArcaneCharacter* Avatar = Cast<AArcaneCharacter>(GetPawn()))
	{
		if (Avatar->IsMatchLocked() || IsSpectating())
		{
			return;
		}

		if (UArcaneAbilitySystemComponent* ASC = Avatar->GetArcaneASC())
		{
			ASC->AbilityLocalInputReleased(InputID);
		}
	}
}

void AArcanePlayerController::RequestMatchCommand(EArcaneMatchCommand Command, int32 Param)
{
	switch (Command)
	{
	case EArcaneMatchCommand::ChangeTeam:
		ServerRequestChangeTeam(Param);
		break;
	case EArcaneMatchCommand::StartMatch:
		ServerRequestStartMatch();
		break;
	case EArcaneMatchCommand::Rematch:
		ServerRequestRematch();
		break;
	case EArcaneMatchCommand::BackToLobby:
		ServerRequestBackToLobby();
		break;
	case EArcaneMatchCommand::AddBot:
		ServerRequestAddBot(Param);
		break;
	case EArcaneMatchCommand::RemoveBot:
		ServerRequestRemoveBot(Param);
		break;
	case EArcaneMatchCommand::BalanceBots:
		ServerRequestBalanceBots();
		break;
	case EArcaneMatchCommand::SetTeamCapacity:
		ServerRequestSetTeamCapacity(Param);
		break;
	case EArcaneMatchCommand::SetRoundsToWin:
		ServerRequestSetRoundsToWin(Param);
		break;
	case EArcaneMatchCommand::None:
	default:
		break;
	}
}


void AArcanePlayerController::ServerRequestChangeTeam_Implementation(int32 NewTeamID)
{
	if (AArcaneGameMode* GM = GetWorld()->GetAuthGameMode<AArcaneGameMode>())
	{
		GM->HandleTeamRequest(GetPlayerState<AArcanePlayerState>(), NewTeamID);
	}
}

void AArcanePlayerController::ServerRequestStartMatch_Implementation()
{
	if (AArcaneGameMode* GM = GetWorld()->GetAuthGameMode<AArcaneGameMode>())
	{
		GM->HandleStartMatchRequest(this);
	}
}

void AArcanePlayerController::ServerRequestRematch_Implementation()
{
	if (AArcaneGameMode* GM = GetWorld()->GetAuthGameMode<AArcaneGameMode>())
	{
		GM->HandleRematchRequest(this);
	}
}

void AArcanePlayerController::ServerRequestBackToLobby_Implementation()
{
	if (AArcaneGameMode* GM = GetWorld()->GetAuthGameMode<AArcaneGameMode>())
	{
		GM->HandleBackToLobbyRequest(this);
	}
}

void AArcanePlayerController::ServerRequestAddBot_Implementation(int32 TeamID)
{
	if (AArcaneGameMode* GM = GetWorld()->GetAuthGameMode<AArcaneGameMode>())
	{
		GM->RequestAddBot(GetPlayerState<AArcanePlayerState>(), TeamID);
	}
}

void AArcanePlayerController::ServerRequestRemoveBot_Implementation(int32 TeamID)
{
	if (AArcaneGameMode* GM = GetWorld()->GetAuthGameMode<AArcaneGameMode>())
	{
		GM->RequestRemoveLastBot(GetPlayerState<AArcanePlayerState>(), TeamID);
	}
}

void AArcanePlayerController::ServerRequestBalanceBots_Implementation()
{
	if (AArcaneGameMode* GM = GetWorld()->GetAuthGameMode<AArcaneGameMode>())
	{
		GM->RequestBalanceBots(GetPlayerState<AArcanePlayerState>());
	}
}

void AArcanePlayerController::ServerRequestSetTeamCapacity_Implementation(int32 NewCapacity)
{
	if (AArcaneGameMode* GM = GetWorld()->GetAuthGameMode<AArcaneGameMode>())
	{
		GM->RequestSetTeamCapacity(GetPlayerState<AArcanePlayerState>(), NewCapacity);
	}
}

void AArcanePlayerController::ServerRequestSetRoundsToWin_Implementation(int32 NewRounds)
{
	if (AArcaneGameMode* GM = GetWorld()->GetAuthGameMode<AArcaneGameMode>())
	{
		GM->RequestSetRoundsToWin(GetPlayerState<AArcanePlayerState>(), NewRounds);
	}
}

void AArcanePlayerController::EnterSpectatorMode()
{
	if (SpectatorComponent == nullptr || SpectatorComponent->IsSpectating())
	{
		return;
	}
	
	if (!SpectatorComponent->EnterSpectatorMode())
	{
		return;
	}
	
	SetIgnoreLookInput(true);  // 锁定视角旋转，观战是固定跟随镜头
	// 隐藏鼠标、沉浸观战；Leave 时由 ApplyViewportKeyboardFocus 复原 GameAndUI
	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
}

void AArcanePlayerController::LeaveSpectatorMode()
{
	if (SpectatorComponent == nullptr || !SpectatorComponent->IsSpectating())
	{
		return;
	}
 
	SpectatorComponent->LeaveSpectatorMode();
 
	ResetIgnoreLookInput();
	ApplyViewportKeyboardFocus();
}

bool AArcanePlayerController::IsSpectating() const
{
	return SpectatorComponent != nullptr && SpectatorComponent->IsSpectating();
}

void AArcanePlayerController::ShowAbilityRangePreview(AActor* SourceActor, float CastRange, float ImpactRadius)
{
	if (!IsLocalController() || RangePreviewWidgetClass == nullptr)
	{
		return;
	}
 
	if (RangePreviewWidget == nullptr)
	{
		RangePreviewWidget = CreateWidget<UArcaneAbilityRangePreviewWidget>(this, RangePreviewWidgetClass);
		if (RangePreviewWidget == nullptr)
		{
			return;
		}
 
		// Z = -1：在战斗 HUD 之下、3D 场景之上；暂停菜单（100）会盖住它
		RangePreviewWidget->AddToViewport(-1);
	}
	
	RangePreviewWidget->ShowPreview(SourceActor, CastRange, ImpactRadius);
}

void AArcanePlayerController::HideAbilityRangePreview()
{
	if (RangePreviewWidget != nullptr)
	{
		RangePreviewWidget->HidePreview();
	}
}

void AArcanePlayerController::OnSpectateNext()
{
	if (SpectatorComponent != nullptr)
	{
		SpectatorComponent->CycleNext();
	}
}

void AArcanePlayerController::OnSpectatePrev()
{
	if (SpectatorComponent != nullptr)
	{
		SpectatorComponent->CyclePrev();
	}
}
