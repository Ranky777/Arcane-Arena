// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcaneAbilityRangePreviewWidget.h"

#include "Arcane_Arena.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Combat/ArcaneCombatFunctionLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CapsuleComponent.h"
#include "Components/Image.h"
#include "GameFramework/Character.h"

// 与 UArcaneTargetingSelectionTask_Lock 的默认光标参数一致
static constexpr float PreviewCursorTraceDistance = 4000.0f;
static constexpr float PreviewCursorFallbackDistance = 2000.0f;


void UArcaneAbilityRangePreviewWidget::ShowPreview(AActor* InSourceActor, float InCastRange, float InImpactRadius)
{
	SourceActor = InSourceActor;
	CastRange = FMath::Max(0.0f, InCastRange);
	ImpactRadius = FMath::Max(0.0f, InImpactRadius);
	bPreviewActive = SourceActor.IsValid() && CastRange > 0.0f;
	
	if (bPreviewActive == false)
	{
		HidePreview();
		return;
	}
	
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UArcaneAbilityRangePreviewWidget::HidePreview()
{
	bPreviewActive = false;
	SourceActor = nullptr;
	
	SetVisibility(ESlateVisibility::Collapsed);
	
	if (Img_CastRange != nullptr)
	{
		Img_CastRange->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (Img_ImpactRange != nullptr)
	{
		Img_ImpactRange->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UArcaneAbilityRangePreviewWidget::NativeOnConstructed()
{
	Super::NativeOnConstructed();
	
	if (Canvas_Root == nullptr)
	{
		UE_LOG(LogArcane, Warning, TEXT("[RangePreview] WBP 缺少 Canvas_Root，圆的位置可能不可控"));
	}
	
	auto SetupCircle = [](UImage* Image, const FLinearColor& Color, int32 ZOrder)
	{
		if (Image == nullptr)
		{
			return;
		}
		
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Image->Slot))
		{
			CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
			CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f)); // 位置 = 圆心，尺寸 = 直径
			CanvasSlot->SetZOrder(ZOrder);
		}
		
		Image->SetColorAndOpacity(Color);
		Image->SetVisibility(ESlateVisibility::Collapsed);
	};
	
	SetupCircle(Img_CastRange, FLinearColor(0.20f, 0.62f, 1.0f, 0.65f), 0);
	SetupCircle(Img_ImpactRange, FLinearColor(1.0f, 0.62f, 0.18f, 0.85f), 1);
	
	SetVisibility(ESlateVisibility::Collapsed);
}

void UArcaneAbilityRangePreviewWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	
	if (bPreviewActive == false)
	{
		return;
	}
 
	if (SourceActor.IsValid() == false)
	{
		HidePreview();
		return;
	}
 
	const FVector CastCenter = ResolveCastCenter();
	const FVector AimPoint = ResolveAimPoint(CastCenter);
 
	UpdateCircle(Img_CastRange, CastCenter, CastRange);
	UpdateCircle(Img_ImpactRange, AimPoint, ImpactRadius);
}

void UArcaneAbilityRangePreviewWidget::UpdateCircle(UImage* Image, const FVector& WorldCenter, float WorldRadius)
{
	if (Image == nullptr)
	{
		return;
	}
 
	APlayerController* PC = GetOwningPlayer();
	if (PC == nullptr || WorldRadius <= 0.0f)
	{
		Image->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	
	FVector2D CenterScreen;
	if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, WorldCenter, CenterScreen, false))
	{
		// 圆心在相机背后：整圈隐藏
		Image->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	
	// 用相机水平右方向和"地面上的屏幕竖直方向"投影两个边缘点，
	// 得到地面圆在屏幕上的外接椭圆。相机有俯角，所以是椭圆而不是正圆，这是对的。
	const FRotator CameraRotation = PC->PlayerCameraManager
		? PC->PlayerCameraManager->GetCameraRotation()
		: PC->GetControlRotation();
	
	FVector RightAxis = FRotationMatrix(CameraRotation).GetUnitAxis(EAxis::Y);
	RightAxis.Z = 0.0f;
	if (!RightAxis.Normalize())
	{
		RightAxis = FVector::RightVector;
	}
 
	const FVector GroundForward = FVector::CrossProduct(RightAxis, FVector::UpVector).GetSafeNormal();
 
	FVector2D EdgeRight;
	FVector2D EdgeForward;
	const bool bRightOk = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
		PC, WorldCenter + RightAxis * WorldRadius, EdgeRight, false);
	const bool bForwardOk = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
		PC, WorldCenter + GroundForward * WorldRadius, EdgeForward, false);
	
	if (!bRightOk || !bForwardOk)
	{
		Image->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
 
	const float RadiusX = FMath::Abs(EdgeRight.X - CenterScreen.X);
	const float RadiusY = FMath::Abs(EdgeForward.Y - CenterScreen.Y);
	
	if (RadiusX < 1.0f || RadiusY < 1.0f)
	{
		Image->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
 
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Image->Slot))
	{
		CanvasSlot->SetPosition(CenterScreen);
		CanvasSlot->SetSize(FVector2D(RadiusX * 2.0f, RadiusY * 2.0f));
	}
	
	Image->SetVisibility(ESlateVisibility::HitTestInvisible);
}

FVector UArcaneAbilityRangePreviewWidget::ResolveCastCenter() const
{
	FVector Center = SourceActor->GetActorLocation();
	
	// 圆画在脚下，不画在胶囊中心；竞技场地形是平的，够用
	if (const ACharacter* Character = Cast<ACharacter>(SourceActor.Get()))
	{
		if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			Center.Z -= Capsule->GetScaledCapsuleHalfHeight();
		}
	}
	
	return Center;
}

FVector UArcaneAbilityRangePreviewWidget::ResolveAimPoint(const FVector& CastCenter) const
{
	const APlayerController* PC = GetOwningPlayer();
	FVector AimPoint = CastCenter;
 
	if (PC == nullptr ||
		UArcaneCombatFunctionLibrary::TryGetCursorGroundPoint(PC, SourceActor.Get(),
			PreviewCursorTraceDistance, PreviewCursorFallbackDistance, AimPoint) == false)
	{
		return CastCenter;
	}
	
	// 与服务器 LaunchHomingAttack 的钳制语义一致：宁可贴在射程边界，也不画到圈外
	const FVector Delta = AimPoint - CastCenter;
	if (Delta.SizeSquared2D() > FMath::Square(CastRange))
	{
		AimPoint = CastCenter + Delta.GetSafeNormal2D() * CastRange;
	}
 
	return AimPoint;
}
