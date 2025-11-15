#include "CPawnLifecycleSubsystem.h"
#include "00_Character/00_Player/CPlayerCharacter.h"
	
void UCPawnLifecycleSubsystem::NotifyPlayerRespawned(ACPlayerCharacter* NewPlayer)
{
	if (!IsValid(NewPlayer))
	{
		UE_LOG(LogTemp, Warning, TEXT("[PawnLifecycleSubsystem] Tried to broadcast respawn with invalid player"));
		return;
	}
		
	PlayerRespawnedEvent.Broadcast(NewPlayer);
}