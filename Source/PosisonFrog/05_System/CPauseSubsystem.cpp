// Fill out your copyright notice in the Description page of Project Settings.


#include "CPauseSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
    
void UCPauseSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    PauseState = EGamePauseState::Normal;
    bCanPause = true;
    Pauser.Reset();
}

void UCPauseSubsystem::Deinitialize()
{
    Pauser.Reset();
    PauseState = EGamePauseState::Normal;
    bCanPause = true;

    Super::Deinitialize();
}

bool UCPauseSubsystem::RequestPause(APlayerController* RequestingController)
{
    if (!bCanPause)
    {
        return false;
    }
    
    if (PauseState == EGamePauseState::Paused)
    {
        // Already paused – nothing to do.
        return true;
    }
    if (!InternalPause(RequestingController))
    {
        return false;
    }

    ApplyPauseState(EGamePauseState::Paused);
    return true;
}

bool UCPauseSubsystem::RequestResume(APlayerController* RequestingController)
{
    if (PauseState == EGamePauseState::Normal)
    {
        return true;
    }

    if (!InternalResume(RequestingController))
    {
        return false;
    }

    ApplyPauseState(EGamePauseState::Normal);
    return true;
}

bool UCPauseSubsystem::RequestTogglePause(APlayerController* RequestingController)
{
    if (PauseState == EGamePauseState::Paused)
    {
        return RequestResume(RequestingController);
    }
    return RequestPause(RequestingController);
}

void UCPauseSubsystem::SetCanPause(bool bInCanPause)
{
    if (bCanPause == bInCanPause)
    {
        return;
    }

    bCanPause = bInCanPause;
    OnCanPauseChanged.Broadcast(bCanPause);

    if (!bCanPause && PauseState == EGamePauseState::Paused)
    {
        // Force resume if pause became disallowed.
        RequestResume(Pauser.Get());
    }
}

bool UCPauseSubsystem::InternalPause(APlayerController* RequestingController)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    APlayerController* Controller = RequestingController;
    if (!Controller)
    {
        Controller = World->GetFirstPlayerController();
    }

    if (!Controller)
    {
        return false;
    }

    if (!Controller->SetPause(true))
    {
        return false;
    }

    Pauser = Controller;
    return true;
}

bool UCPauseSubsystem::InternalResume(APlayerController* RequestingController)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    APlayerController* Controller = RequestingController;
    if (!Controller)
    {
        Controller = Pauser.Get();
    }
    if (!Controller)
    {
        Controller = World->GetFirstPlayerController();
    }

    if (!Controller)
    {
        return false;
    }

    Controller->SetPause(false);
    Pauser.Reset();
    return true;
}

void UCPauseSubsystem::ApplyPauseState(EGamePauseState NewState)
{
    if (PauseState == NewState)
    {
        return;
    }

    PauseState = NewState;
    OnPauseStateChanged.Broadcast(PauseState);
}