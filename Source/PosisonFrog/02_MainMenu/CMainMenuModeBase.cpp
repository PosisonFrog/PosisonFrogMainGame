// Fill out your copyright notice in the Description page of Project Settings.


#include "CMainMenuModeBase.h"

#include "CMainMenuController.h"

ACMainMenuModeBase::ACMainMenuModeBase()
{
	PlayerControllerClass = ACMainMenuController::StaticClass();
	DefaultPawnClass = nullptr;
}
