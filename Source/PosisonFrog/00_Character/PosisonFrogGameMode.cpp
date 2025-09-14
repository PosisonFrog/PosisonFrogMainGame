// Copyright Epic Games, Inc. All Rights Reserved.

#include "PosisonFrogGameMode.h"
#include "PosisonFrogCharacter.h"
#include "UObject/ConstructorHelpers.h"

APosisonFrogGameMode::APosisonFrogGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
