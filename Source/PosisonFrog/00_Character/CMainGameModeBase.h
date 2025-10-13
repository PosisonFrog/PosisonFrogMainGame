// CMainGameModeBase.h
// gamePlay용

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CMainGameModeBase.generated.h"

class ACPlayerController;
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

	UFUNCTION()
	void OnPlayerDeath(ACPlayerController* PlayerController);

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION() void ReturnToMenu();
	
private:
	UPROPERTY(EditAnywhere, Category = "Item")
	TSubclassOf<ACHealOrb> HealOrbClass;

	UPROPERTY(EditDefaultsOnly, Category = "Game|Death", meta = (AllowPrivateAccess = "true"))
	FName MainMenuLevelName = TEXT("MainMenu");

	UPROPERTY(EditDefaultsOnly, Category = "Game|Death", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float DeathReturnDelay = 5.0f;

	FTimerHandle TimerHandle_ReturnToMenu;
};
