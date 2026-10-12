// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GAS/ArcaneGameplayAbility.h"
#include "ArcanePlayerController.generated.h"

class UInputMappingContext;
class UInputAction;

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
	
public:
	UPROPERTY(EditDefaultsOnly, Category = "Arcane|UI")
	TObjectPtr<UInputAction> PauseAction = nullptr;
	
	UPROPERTY(EditDefaultsOnly, Category = "Arcane|UI")
	TSubclassOf<UUserWidget> PauseMenuWidgetClass = nullptr;
	
	UPROPERTY()
	TObjectPtr<UUserWidget> PauseMenuWidget = nullptr;
	
	bool bPauseMenuOpen = false;
	
	UFUNCTION(BlueprintCallable, Category = "Arcane|UI")
	void TogglePauseMenu();
	
	// 暂停菜单的「继续」按钮（C++ 基类）直接调用
	void ClosePauseMenu();
	
private:
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
};
