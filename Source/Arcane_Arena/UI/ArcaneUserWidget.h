// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "Blueprint/UserWidget.h"
#include "ArcaneUserWidget.generated.h"

class UAbilitySystemComponent;
struct FOnAttributeChangeData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnArcaneAttributeChanged, FGameplayAttribute, Attribute, float, NewValue, float, OldValue);

/**
 * UI 基类：负责把 GAS 属性变化桥接为显示数据（C++ 虚钩子 + 蓝图动态委托双通道）。
 */
UCLASS()
class ARCANE_ARENA_API UArcaneUserWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 绑定 ASC 的 Health/MaxHealth 属性变化（可安全重入：换 ASC 时自动解绑旧的）
	UFUNCTION(BlueprintCallable, Category = "Arcane|UI")
	void InitFromASC(UAbilitySystemComponent* ASC);
	
	// 注册要监听的属性。InitFromASC 之前调用也不会丢（先缓存，绑定 ASC 后补绑）
	UFUNCTION(BlueprintCallable, Category = "Arcane|UI")
	void BindAttribute(const FGameplayAttribute& Attribute);

	UPROPERTY(BlueprintAssignable, Category = "Arcane|UI")
	FOnArcaneAttributeChanged OnAttributeChanged;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	
	// C++ 子类的构造钩子（在蓝图 Construct 逻辑之后由基类调用）
	virtual void NativeOnConstructed() {}
	
	// C++ 子类的属性变化钩子；蓝图动态委托照常广播，互不影响
	virtual void NativeOnAttributeChanged(const FGameplayAttribute& Attribute, float NewValue, float OldValue) {}
	
	// HUD 构造完成后由蓝图实现：用 GameState 当前 MatchPhase 主动驱动一次面板刷新。
	// 解决客户端初始复制时 OnRep 不触发（值等于默认值）、以及服务端广播早于 HUD
	// 绑定的竞态，保证进房/进图时选阵营/结算面板的初始显隐一定正确。
	UFUNCTION(BlueprintImplementableEvent, Category = "Arcane|UI")
	void OnUserWidgetConstructed();
	
	UPROPERTY(BlueprintReadOnly, Category = "Arcane|UI")
	TObjectPtr<UAbilitySystemComponent> BoundASC;

private:
	void HandleAttributeChanged(const FOnAttributeChangeData& Data);
	void BroadcastAttribute(const FGameplayAttribute& Attribute, float NewValue, float OldValue);
	void UnbindAll();
	
	TMap<FGameplayAttribute, FDelegateHandle> Handles;
	TArray<FGameplayAttribute> PendingAttributes; // InitFromASC 之前注册的属性缓存
};
