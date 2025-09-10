// Fill out your copyright notice in the Description page of Project Settings.


#include "00_Character/00_Player/CPlayerController.h"

#include "EnhancedInputSubsystems.h"

ACPlayerController::ACPlayerController()
{
}

void ACPlayerController::BeginPlay()
{
	Super::BeginPlay();

	FInputModeGameOnly Mode;
	SetInputMode(Mode);
	bShowMouseCursor = false;

	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (auto* Subsys = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
		{
			if (bClearPreviousMappings)
				Subsys->ClearAllMappings();

			if (DefaultMappingContext)
			{
				Subsys->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}
}
