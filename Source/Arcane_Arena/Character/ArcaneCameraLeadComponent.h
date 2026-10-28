// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ArcaneCameraLeadComponent.generated.h"

class USpringArmComponent;
/**
 * 本地玩家相机 lead 组件。
 * 复刻 BP_CombatCharacter.EventTick 的俯视相机逻辑：
 * - 鼠标光标决定视角 Yaw
 * - 相机 Boom 朝光标方向做水平 lead
 * - 固定 Pitch -65
 *
 * 只对本地 PlayerController 控制的角色生效；Bot / 远端代理不参与。
 */
UCLASS(ClassGroup=(Arcane), meta=(BlueprintSpawnableComponent))
class ARCANE_ARENA_API UArcaneCameraLeadComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UArcaneCameraLeadComponent();
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arcane|Camera")
	float ZoomMin = 600.0f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arcane|Camera")
	float ZoomMax = 1600.0f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arcane|Camera")
	float CameraLeadMax = 300.0f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arcane|Camera")
	float LookAtHeight = 100.0f;
 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arcane|Camera")
	float LeadInterpSpeed = 8.0f;
 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arcane|Camera")
	float RotationInterpSpeed = 360.0f;
 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arcane|Camera")
	float FixedPitch = -65.0f;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
	
private:
	TWeakObjectPtr<ACharacter> OwnerCharacter;
	TWeakObjectPtr<USpringArmComponent> CameraBoom;
};
