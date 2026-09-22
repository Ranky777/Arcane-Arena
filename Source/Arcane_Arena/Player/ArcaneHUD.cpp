// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneHUD.h"

#include "Blueprint/UserWidget.h"

void AArcaneHUD::BeginPlay()
{
	Super::BeginPlay();
	
	// 只在本地玩家的 HUD 里创建（ListenServer 主机 + 每个客户端各一份）
	if (GlobalHUDWidgetClass != nullptr && HUDWidget == nullptr)
	{
		HUDWidget = CreateWidget<UUserWidget>(GetWorld(), GlobalHUDWidgetClass);
		if (HUDWidget != nullptr)
		{
			HUDWidget->AddToViewport();
		}
	}
}
