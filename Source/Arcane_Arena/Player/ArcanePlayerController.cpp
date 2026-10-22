// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcanePlayerController.h"

#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Blueprint/UserWidget.h"
#include "Character/ArcaneBotCharacter.h"
#include "Character/ArcaneCharacter.h"
#include "Engine/GameViewportClient.h"
#include "Framework/ArcaneGameMode.h"
#include "Framework/ArcaneGameState.h"
#include "GAS/ArcaneAbilitySystemComponent.h"
#include "Player/ArcanePlayerState.h"   // Server RPC 里 GetPlayerState<AArcanePlayerState>() 需要完整类型
#include "UI/ArcaneAbilityRangePreviewWidget.h"
#include "Widgets/SViewport.h"


AArcanePlayerController::AArcanePlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Crosshairs;
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
	if (bIsSpectating)
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
		if (Avatar->IsMatchLocked() || bIsSpectating)
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
		if (Avatar->IsMatchLocked() || bIsSpectating)
		{
			return;
		}

		if (UArcaneAbilitySystemComponent* ASC = Avatar->GetArcaneASC())
		{
			ASC->AbilityLocalInputReleased(InputID);
		}
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
	if (bIsSpectating)
	{
		return;
	}
	
	TArray<AArcaneCharacter*> Teammates;
	BuildLivingTeammates(Teammates);
	
	// 队里没活人（我方最后一名）→ 不进观战，原地等回合结算。
	if (Teammates.Num() == 0)
	{
		return;
	}
	
	bIsSpectating = true;
	SetIgnoreLookInput(true);  // 锁定视角旋转，观战是固定跟随镜头
	// 隐藏鼠标、沉浸观战；Leave 时由 ApplyViewportKeyboardFocus 复原 GameAndUI
	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
	
	ViewTeammate(Teammates[0]);
	
	GetWorldTimerManager().SetTimer(
		SpectateValidateTimer, this,
		&AArcanePlayerController::ValidateSpectateTarget, 
		0.25f, true);
}

void AArcanePlayerController::LeaveSpectatorMode()
{
	if (bIsSpectating == false)
	{
		return;
	}
	
	bIsSpectating = false;
	
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(SpectateValidateTimer);
	}
	
	ResetIgnoreLookInput();
	OnSpectateTargetChanged.Broadcast(nullptr);
	
	// 交回自己的新身体（重生路径下 OnPossess 已 Possess 到新 Pawn）
	if (APawn* Self = GetPawn())
	{
		SetViewTargetWithBlend(Self, 0.4f);
	}
	
	ApplyViewportKeyboardFocus();
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

int32 AArcanePlayerController::GetLocalTeamID() const
{
	if (const AArcanePlayerState* PS = GetPlayerState<AArcanePlayerState>())
	{
		return PS->TeamID;
	}
	
	return INDEX_NONE;
}

void AArcanePlayerController::BuildLivingTeammates(TArray<AArcaneCharacter*>& OutTeammates) const
{
	OutTeammates.Reset();
	const int32 MyTeam = GetLocalTeamID();
	
	if (MyTeam == INDEX_NONE || GetWorld() == nullptr)
	{
		return;
	}
	
	// 客户端只能靠复制下来的 Character.bIsDead / PlayerState.TeamID 枚举；
	// 远程玩家的 PlayerController 不复制，但 Pawn(Character) 是复制的，够用。
	for (TActorIterator<AArcaneCharacter> It(GetWorld()); It; ++It)
	{
		AArcaneCharacter* Candidate = *It;
		if (Candidate == nullptr || Candidate->bIsDead)
		{
			continue;
		}
		
		const AArcanePlayerState* PS = Candidate->GetPlayerState<AArcanePlayerState>();
		if (PS == nullptr || PS->TeamID != MyTeam)
		{
			continue;
		}
		
		// Bot 也是一等 PlayerState，天然进候选池；没有 Pawn 的候选略过
		if (Candidate == nullptr)
		{
			continue;
		}
		
		OutTeammates.Add(Candidate);
	}
}

void AArcanePlayerController::ViewTeammate(AArcaneCharacter* Target)
{
	if (Target == nullptr)
	{
		return;
	}
	
	CurrentSpectateTarget = Target;
	
	// BlendTime 0.4s 平滑切入，观战不突兀。
	SetViewTargetWithBlend(Target, 0.4f);
	OnSpectateTargetChanged.Broadcast(Target);
}

void AArcanePlayerController::OnSpectateNext()
{
	CycleSpectateTarget(+1);
}

void AArcanePlayerController::OnSpectatePrev()
{
	CycleSpectateTarget(-1);
}

void AArcanePlayerController::CycleSpectateTarget(int32 Direction)
{
	if (bIsSpectating == false)
	{
		return;
	}
	
	TArray<AArcaneCharacter*> Teammates;
	BuildLivingTeammates(Teammates);
	if (Teammates.Num() == 0)
	{
		return;
	}
	
	int32 CurrentIndex = 0;
	for (int32 i = 0; i < Teammates.Num(); ++i)
	{
		if (Teammates[i] == CurrentSpectateTarget.Get())
		{
			CurrentIndex = i;
			break;
		}
	}
	
	const int32 N = Teammates.Num();
	const int32 NextIndex = ((CurrentIndex + Direction) % N + N) % N; // 环绕
	
	ViewTeammate(Teammates[NextIndex]);
}

void AArcanePlayerController::ValidateSpectateTarget()
{
	if (bIsSpectating == false)
	{
		if (GetWorld() != nullptr)
		{
			GetWorldTimerManager().ClearTimer(SpectateValidateTimer);
		}
		
		return;
	}
	
	// 当前目标还在存活列表里 → 什么都不做
	TArray<AArcaneCharacter*> Teammates;
	BuildLivingTeammates(Teammates);
	for (AArcaneCharacter* T : Teammates)
	{
		if (T == CurrentSpectateTarget.Get())
		{
			return;
		}
	}
	
	// 目标已死/消失：有别的活人就推进到第一个
	if (Teammates.Num() > 0)
	{
		ViewTeammate(Teammates[0]);
		return;
	}
	
	// 全灭：保留看最后的尸体（若还有效）直到回合结束，不回切到自己尸体
	if (!CurrentSpectateTarget.IsValid())
	{
		LeaveSpectatorMode();
	}
}
