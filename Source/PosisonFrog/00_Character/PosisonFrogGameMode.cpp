// Copyright Epic Games, Inc. All Rights Reserved.

#include "PosisonFrogGameMode.h"
#include "PosisonFrogCharacter.h"
#include "UObject/ConstructorHelpers.h"

APosisonFrogGameMode::APosisonFrogGameMode()
{
	// set default pawn class to our Blueprinted character
    static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/Bluprints/Character/Player/BP_CPlayerCharacter.BP_CPlayerCharacter_C"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
