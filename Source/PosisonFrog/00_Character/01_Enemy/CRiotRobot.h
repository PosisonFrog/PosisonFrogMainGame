#pragma once
#include "CoreMinimal.h"
#include "00_Character/01_Enemy/CEnemyCharacterBase.h"
#include "CRiotRobot.generated.h"

/** 일반형: 1.0s 주기 근접 1타 */
UCLASS()
class POSISONFROG_API ACRiotRobot : public ACEnemyCharacterBase
{
	GENERATED_BODY()
public:
	ACRiotRobot();
protected:
	virtual void DoAttack() override;
};

