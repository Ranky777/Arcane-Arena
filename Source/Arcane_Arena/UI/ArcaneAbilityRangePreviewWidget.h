// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArcaneUserWidget.h"
#include "ArcaneAbilityRangePreviewWidget.generated.h"

class UCanvasPanel;
class UImage;
/**
 * 远程技能按住预览：外圈 = 施法射程，内圈 = 弹体碰撞/影响半径。
 * WBP 只放版式（Canvas_Root / Img_CastRange / Img_ImpactRange），
 * 世界坐标投影、显隐、颜色全部在 C++；仅本地施法者可见。
 */
UCLASS()
class ARCANE_ARENA_API UArcaneAbilityRangePreviewWidget : public UArcaneUserWidget
{
	GENERATED_BODY()
	
public:
	// SourceActor = 施法者 Pawn；Range/ImpactRadius 来自 AbilityTable
	void ShowPreview(AActor* InSourceActor, float InCastRange, float InImpactRadius);
	void HidePreview();
	
protected:
	virtual void NativeOnConstructed() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> Canvas_Root;
 
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_CastRange;
 
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_ImpactRange;
	
private:
	// 把世界圆投影成屏幕椭圆（相机固定俯角，用两条地面轴算长短半径）
	void UpdateCircle(UImage* Image, const FVector& WorldCenter, float WorldRadius);
	
	// 外圈圆心：角色脚下。内圈圆心：光标地面点，并钳进射程
	FVector ResolveCastCenter() const;
	FVector ResolveAimPoint(const FVector& CastCenter) const;
	
	TWeakObjectPtr<AActor> SourceActor;
	float CastRange = 0.0f;
	float ImpactRadius = 0.0f;
	bool bPreviewActive = false;
};
