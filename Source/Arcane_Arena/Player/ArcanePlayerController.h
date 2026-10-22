// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GAS/ArcaneGameplayAbility.h"
#include "ArcanePlayerController.generated.h"

class UArcaneAbilityRangePreviewWidget;
class AArcaneCharacter;
class UInputMappingContext;
class UInputAction;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnArcaneSpectateTargetChanged, class AActor*, NewTarget);

USTRUCT(BlueprintType)
struct FArcaneAbilityInputMapping
{
	GENERATED_BODY()
	
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<UInputAction> InputAction = nullptr;
	
	UPROPERTY(EditDefaultsOnly)
	EArcaneAbilityInputID AbilityID = EArcaneAbilityInputID::None;
};

/**
 * 
 */
UCLASS()
class ARCANE_ARENA_API AArcanePlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	AArcanePlayerController();
	
protected:
	virtual void BeginPlay() override; 
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;
	
	UPROPERTY(EditDefaultsOnly, Category = "Arcane|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext = nullptr; // 配 IMC_Default + IMC_Combat 合并版
	
	// 能力输入映射（在 BP_ArcanePlayerController 里逐条配 IA→InputID）
	UPROPERTY(EditDefaultsOnly, Category = "Arcane|Input")
	TArray<FArcaneAbilityInputMapping> AbilityInputMappings;
	
	// —— 观战：切人输入（本地表现，不涉权威）。IA_SpectateNext 绑 E + 滚轮下，
	// IA_SpectatePrev 绑 Q + 滚轮上，都塞进现有 DefaultMappingContext 即可。
	UPROPERTY(EditDefaultsOnly, Category = "Arcane|Spectate")
	TObjectPtr<UInputAction> SpectateNextAction = nullptr;
	
	UPROPERTY(EditDefaultsOnly, Category = "Arcane|Spectate")
	TObjectPtr<UInputAction> SpectatePrevAction = nullptr;
	
public:
	UPROPERTY(EditDefaultsOnly, Category = "Arcane|UI")
	TObjectPtr<UInputAction> PauseAction = nullptr;
	
	UPROPERTY(EditDefaultsOnly, Category = "Arcane|UI")
	TSubclassOf<UUserWidget> PauseMenuWidgetClass = nullptr;
	
	UPROPERTY()
	TObjectPtr<UUserWidget> PauseMenuWidget = nullptr;
	
	// —— 远程技能范围预览（本地表现，仅施法者本人可见）——
	UPROPERTY(EditDefaultsOnly, Category = "Arcane|UI")
	TSubclassOf<UArcaneAbilityRangePreviewWidget> RangePreviewWidgetClass = nullptr;
 
	UPROPERTY(Transient)
	TObjectPtr<UArcaneAbilityRangePreviewWidget> RangePreviewWidget = nullptr;
	
	bool bPauseMenuOpen = false;
	
	UFUNCTION(BlueprintCallable, Category = "Arcane|UI")
	void TogglePauseMenu();
	
	// 暂停菜单的「继续」按钮（C++ 基类）直接调用
	void ClosePauseMenu();
	
	// —— 观战系统（本地视角，客户端专属）——
	// 死亡时由 AArcaneCharacter::ApplyDeathPresentation 调用（仅本地玩家）。
	UFUNCTION(BlueprintCallable, Category = "Arcane|Spectate")
	void EnterSpectatorMode();
	
	// 重生 OnPossess 时自动调用；也可被 UI/调试手动退出。
	UFUNCTION(BlueprintCallable, Category = "Arcane|Spectate")
	void LeaveSpectatorMode();
	
	UFUNCTION(BlueprintPure, Category = "Arcane|Spectate")
	bool IsSpectating() const { return bIsSpectating; }
	
	void ShowAbilityRangePreview(AActor* SourceActor, float CastRange, float ImpactRadius);
	void HideAbilityRangePreview();
	
	// 目标切换事件，供 HUD 显示"正在观战：xxx"
	FOnArcaneSpectateTargetChanged OnSpectateTargetChanged;
	
private:
	// 观战内部
	bool bIsSpectating = false;
	
	FTimerHandle SpectateValidateTimer;
	
	TWeakObjectPtr<AActor> CurrentSpectateTarget;
	
	int32 GetLocalTeamID() const;
	
	void BuildLivingTeammates(TArray<AArcaneCharacter*>& OutTeammates) const;
	
	void ViewTeammate(AArcaneCharacter* Target);
	
	UFUNCTION()
	void OnSpectateNext();
	
	UFUNCTION()
	void OnSpectatePrev();
	
	void CycleSpectateTarget(int32 Direction);
	
	void ValidateSpectateTarget();
	
	void BindAbilityInputs();

	// 键盘焦点必须落在游戏视口上，Enhanced Input 的按键才会送进 PlayerInput；
	// 焦点停在其它 UMG 上时所有键盘输入都会静默失效。
	void ApplyViewportKeyboardFocus();

	// 必须是 UFUNCTION：BindAction 带参重载在运行时按 UFunction 名字解析
	UFUNCTION()
	void OnAbilityInputPressed(int32 InputID);

	UFUNCTION()
	void OnAbilityInputReleased(int32 InputID);
	
public:
	// —— 选阵营 / 结算面板的服务器请求 ——
	// 客户端不能直接改 TeamID / 阶段，一律走 Server RPC，由 GameMode 校验。
	// 只能从「自己的 PC」发起，所以 UI 里必须 GetOwningPlayer → Cast 到本 PC。
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Arcane|Match")
	void ServerRequestChangeTeam(int32 NewTeamID);
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Arcane|Match")
	void ServerRequestStartMatch();
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Arcane|Match")
	void ServerRequestRematch();
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Arcane|Match")
	void ServerRequestBackToLobby();
	
	// 房主加/删 Bot。权限校验在 GameMode（Warmup 阶段/房主/满员），这里只转发。
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Arcane|Match")
	void ServerRequestAddBot(int32 TeamID);
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Arcane|Match")
	void ServerRequestRemoveBot(int32 TeamID);
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Arcane|Match")
	void ServerRequestBalanceBots();
	
	// 房主设置每队人数上限（服务端校验阶段/权限并收窄到出生点可支撑的范围）
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Arcane|Match")
	void ServerRequestSetTeamCapacity(int32 NewCapacity);
	
	// 房主设置"先赢几局取胜"（服务端校验阶段/权限并钳制到可调上限）
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Arcane|Match")
	void ServerRequestSetRoundsToWin(int32 NewRounds);
};
