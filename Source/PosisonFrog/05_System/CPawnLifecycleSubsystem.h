
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CPawnLifecycleSubsystem.generated.h"

class ACPlayerCharacter;

/**
 * Subsystem that centralizes gameplay events related to player pawn lifecycle
 * (e.g. respawn). Enemy AI can subscribe to receive notifications whenever
 * a new player pawn becomes active so they can refresh any cached references.
*/
UCLASS()
class POSISONFROG_API UCPawnLifecycleSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPlayerRespawnedNative, ACPlayerCharacter* /*NewPlayer*/);
	
	/** Broadcasts when a new player pawn has been respawned and is ready. */
	FOnPlayerRespawnedNative& OnPlayerRespawned() { return PlayerRespawnedEvent; }
	
	/** Notify all listeners that the player has respawned. */
	void NotifyPlayerRespawned(ACPlayerCharacter* NewPlayer);
	
private:
	FOnPlayerRespawnedNative PlayerRespawnedEvent;
};
