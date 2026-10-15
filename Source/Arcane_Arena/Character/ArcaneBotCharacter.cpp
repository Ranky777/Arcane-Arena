#include "ArcaneBotCharacter.h"

void AArcaneBotCharacter::PostInitializeComponents()
{
	AutoPossessAI = EAutoPossessAI::Disabled;
	
	Super::PostInitializeComponents();
}
