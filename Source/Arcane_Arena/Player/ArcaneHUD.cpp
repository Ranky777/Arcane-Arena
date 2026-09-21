// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneHUD.h"

#include "Blueprint/UserWidget.h"

void AArcaneHUD::BeginPlay()
{
	Super::BeginPlay();
	
	if (GlobalHUDWidgetClass != nullptr)
	{
		if (UUserWidget* WB = CreateWidget<UUserWidget>(GetWorld(), GlobalHUDWidgetClass))
		{
			WB->AddToViewport();
		}
	}
}
