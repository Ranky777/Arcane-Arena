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
 * UI 基类：负责把 GAS 属性变化桥接为蓝图可绑定的显示数据。
 * WBP 在 Construct 时调用 InitFromASC（传本机 PlayerState 的 ASC）
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
	virtual void NativeDestruct() override;
	
	UPROPERTY(BlueprintReadOnly, Category = "Arcane|UI")
	TObjectPtr<UAbilitySystemComponent> BoundASC;

private:
	void HandleAttributeChanged(const FOnAttributeChangeData& Data);
	void UnbindAll();
	
	TMap<FGameplayAttribute, FDelegateHandle> Handles;
	TArray<FGameplayAttribute> PendingAttributes; // InitFromASC 之前注册的属性缓存
};
