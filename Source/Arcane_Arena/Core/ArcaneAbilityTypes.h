#pragma once

#include "CoreMinimal.h"
#include "ArcaneAbilityTypes.generated.h"

UENUM(BlueprintType)
enum EArcaneAbilityInputID : uint8
{
	None			UMETA(DisplayName = "None"),
	Confirm			UMETA(DisplayName = "Confirm"),
	Cancel			UMETA(DisplayName = "Cancel"),
	PrimaryAbility	UMETA(DisplayName = "Primary Ability"),
	Dash			UMETA(DisplayName = "Dash"),
	Ultimate		UMETA(DisplayName = "Ultimate")
};
