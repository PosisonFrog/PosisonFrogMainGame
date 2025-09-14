// CMainGameModeBase.h
// gamePlay용

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CMainGameModeBase.generated.h"

class ACHealOrb;
/**
 * 게임플레이 기본 GameMode
 * - DefaultPawn: ACPlayerCharacter
 * - PlayerController: ACPlayerController
 */
UCLASS(config = Game)
class POSISONFROG_API ACMainGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACMainGameModeBase();

private:
	UPROPERTY(EditAnywhere)
	TSubclassOf<ACHealOrb> HealOrbClass;
	
protected:
	virtual void BeginPlay() override;
};
