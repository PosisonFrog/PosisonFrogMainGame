// Fill out your copyright notice in the Description page of Project Settings.


#include "CMainGameModeBase.h"
#include "00_Character/00_Player/CPlayerController.h"
#include "00_Character/00_Player/CPlayerCharacter.h"

ACMainGameModeBase::ACMainGameModeBase()
{
	PlayerControllerClass = ACMainGameModeBase::StaticClass();
	DefaultPawnClass = ACPlayerCharacter::StaticClass();
}
