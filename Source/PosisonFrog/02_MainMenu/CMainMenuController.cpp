// Fill out your copyright notice in the Description page of Project Settings.


#include "CMainMenuController.h"

#include "01_Widget/CMainMenuWidget.h"
#include "Blueprint/UserWidget.h"

ACMainMenuController::ACMainMenuController()
{
	bShowMouseCursor = true;
}

void ACMainMenuController::BeginPlay()
{
	Super::BeginPlay();

	if (MainMenuWidgetClass)
	{
		MainMenuWidget = CreateWidget<UCMainMenuWidget>(this, MainMenuWidgetClass);

		MainMenuWidget->AddToViewport();
		FInputModeUIOnly InputModeData;
		InputModeData.SetWidgetToFocus(MainMenuWidget->TakeWidget());
		InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputModeData);
	}
}
