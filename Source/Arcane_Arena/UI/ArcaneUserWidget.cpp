// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneUserWidget.h"

#include "AbilitySystemComponent.h"

void UArcaneUserWidget::BindAttribute(const FGameplayAttribute& Attribute)
{
	if (!Attribute.IsValid())
	{
		return;
	}
	
	if (BoundASC != nullptr)
	{
		if (Handles.Contains(Attribute))
		{
			return;
		}
		else
		{
			Handles.Add(Attribute, BoundASC->GetGameplayAttributeValueChangeDelegate(Attribute).AddUObject(this, &UArcaneUserWidget::HandleAttributeChanged));
			
			// 立即广播一次当前值（初值填充）
			const float Current = BoundASC->GetNumericAttribute(Attribute);
			OnAttributeChanged.Broadcast(Attribute, Current, Current);
		}
	}
	else
	{
		// ASC 还没绑上：先缓存，InitFromASC 后补绑
		PendingAttributes.AddUnique(Attribute);
	}
}

void UArcaneUserWidget::HandleAttributeChanged(const FOnAttributeChangeData& Data)
{
	OnAttributeChanged.Broadcast(Data.Attribute, Data.NewValue, Data.OldValue);
}

void UArcaneUserWidget::UnbindAll()
{
	if (BoundASC != nullptr)
	{
		for (TPair<FGameplayAttribute, FDelegateHandle>& Pair : Handles)
		{
			BoundASC->GetGameplayAttributeValueChangeDelegate(Pair.Key).Remove(Pair.Value);
		}
	}
	Handles.Empty();
	BoundASC = nullptr;
}

void UArcaneUserWidget::InitFromASC(UAbilitySystemComponent* ASC)
{
	if (ASC == nullptr || BoundASC == ASC)
	{
		return;
	}

	// 换 ASC 时先解绑旧的（重生/换 Pawn 场景可安全重入）
	UnbindAll();
	BoundASC = ASC;

	// 补绑 Construct 期间（BoundASC 尚未就绪时）注册的属性
	// BindAttribute 内部会立即广播一次当前值，UI 不会停在 0
	for (const FGameplayAttribute& Attr : PendingAttributes)
	{
		BindAttribute(Attr);
	}
	PendingAttributes.Empty();
}

void UArcaneUserWidget::NativeDestruct()
{
	UnbindAll();

	Super::NativeDestruct();
}
