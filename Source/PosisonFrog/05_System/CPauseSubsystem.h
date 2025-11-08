// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CPauseSubsystem.generated.h"

class APlayerController;

UENUM(BlueprintType)
enum class EGamePauseState : uint8
{
    Normal UMETA(DisplayName = "Normal"),
    Paused UMETA(DisplayName = "Paused")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGamePauseStateDelegate, EGamePauseState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGamePauseCanPauseDelegate, bool, bNewCanPause);

/**
 * Game-wide pause subsystem to centralize pause/resume policy.
 * Responsible for respecting the global bCanPause flag and broadcasting state changes
 * so that gameplay systems (CSC, ULT, AI, etc.) can react consistently.
 */
UCLASS()
class POSISONFROG_API UCPauseSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    
    /** Try to pause the game. Returns false if pausing is disallowed or failed. */
    UFUNCTION(BlueprintCallable, Category = "Pause")
    bool RequestPause(APlayerController* RequestingController);

    /** Try to resume the game. Safe to call even if already resumed. */
    UFUNCTION(BlueprintCallable, Category = "Pause")
    bool RequestResume(APlayerController* RequestingController);

    /** Toggle pause state (Normal <-> Paused). */
    UFUNCTION(BlueprintCallable, Category = "Pause")
    bool RequestTogglePause(APlayerController* RequestingController);

    /** Control whether pause input is accepted. */
    UFUNCTION(BlueprintCallable, Category = "Pause")
    void SetCanPause(bool bInCanPause);

    /** Whether pausing is currently allowed. */
    UFUNCTION(BlueprintPure, Category = "Pause")
    bool CanPause() const { return bCanPause; }

    /** Current pause state. */
    UFUNCTION(BlueprintPure, Category = "Pause")
    EGamePauseState GetPauseState() const { return PauseState; }
    /** Event fired when pause state changes. */
    UPROPERTY(BlueprintAssignable, Category = "Pause")
    FGamePauseStateDelegate OnPauseStateChanged;

    /** Event fired when bCanPause flag changes. */
    UPROPERTY(BlueprintAssignable, Category = "Pause")
    FGamePauseCanPauseDelegate OnCanPauseChanged;

private:
    bool InternalPause(APlayerController* RequestingController);
    bool InternalResume(APlayerController* RequestingController);
    void ApplyPauseState(EGamePauseState NewState);

private:
    EGamePauseState PauseState = EGamePauseState::Normal;
    bool bCanPause = true;
    TWeakObjectPtr<APlayerController> Pauser;
};
