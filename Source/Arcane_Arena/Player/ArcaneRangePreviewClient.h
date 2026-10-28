// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ArcaneRangePreviewClient.generated.h"

// This class does not need to be modified.
UINTERFACE()
class UArcaneRangePreviewClient : public UInterface
{
	GENERATED_BODY()
};

/**
 * 本地范围预览契约。
 * 远程技能只认这个接口，不依赖 AArcanePlayerController。
 */
class ARCANE_ARENA_API IArcaneRangePreviewClient
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual void ShowAbilityRangePreview(AActor* SourceActor, float CastRange, float ImpactRadius) = 0;
	virtual void HideAbilityRangePreview() = 0;
};
