// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CPauseMenuWidget.generated.h"

class UButton;
class UPanelWidget;
class UWidget;
class UOptionsMenuWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPauseMenuSimpleEvent);

/**
 * In-game pause menu widget responsible for driving pause/resume/settings actions.
 * Buttons are expected to be bound in the UMG designer using the names declared below.
 */
UCLASS()
class POSISONFROG_API UPauseMenuWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

    /** Set initial keyboard focus to a sensible default control. */
    UFUNCTION(BlueprintCallable, Category = "PauseMenu")
    void FocusInitial();

    /** Ensure the menu is reset to the main panel (used when pausing closes). */
    UFUNCTION(BlueprintCallable, Category = "PauseMenu")
    void ResetMenuState();

    /** Broadcast when the player presses Resume. */
    UPROPERTY(BlueprintAssignable, Category = "PauseMenu|Event")
    FPauseMenuSimpleEvent OnResumeRequested;

    /** Broadcast when the player requests a restart from the last checkpoint. */
    UPROPERTY(BlueprintAssignable, Category = "PauseMenu|Event")
    FPauseMenuSimpleEvent OnRestartRequested;

    /** Broadcast when the player wants to return to title. */
    UPROPERTY(BlueprintAssignable, Category = "PauseMenu|Event")
    FPauseMenuSimpleEvent OnReturnToTitleRequested;

    /** Broadcast when the player wants to quit the game. */
    UPROPERTY(BlueprintAssignable, Category = "PauseMenu|Event")
    FPauseMenuSimpleEvent OnExitRequested;

protected:
    // ========= Widget bindings =========
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "PauseMenu")
    TObjectPtr<UWidget> MainPanel = nullptr;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "PauseMenu")
    TObjectPtr<UWidget> OptionsPanel = nullptr;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "PauseMenu")
    TObjectPtr<UPanelWidget> OptionsContainer = nullptr;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "PauseMenu")
    TObjectPtr<UButton> Button_Resume = nullptr;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "PauseMenu")
    TObjectPtr<UButton> Button_Settings = nullptr;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "PauseMenu")
    TObjectPtr<UButton> Button_Restart = nullptr;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "PauseMenu")
    TObjectPtr<UButton> Button_ReturnToTitle = nullptr;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "PauseMenu")
    TObjectPtr<UButton> Button_Exit = nullptr;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "PauseMenu")
    TObjectPtr<UButton> Button_BackFromSettings = nullptr;

    // ========= Options widget =========
    UPROPERTY(EditDefaultsOnly, Category = "PauseMenu")
    TSubclassOf<UOptionsMenuWidget> OptionsMenuClass;

private:
    UFUNCTION() void HandleResumeClicked();
    UFUNCTION() void HandleSettingsClicked();
    UFUNCTION() void HandleRestartClicked();
    UFUNCTION() void HandleReturnToTitleClicked();
    UFUNCTION() void HandleExitClicked();
    UFUNCTION() void HandleBackFromSettingsClicked();
    UFUNCTION() void HandleOptionsClosed();

    void EnsureOptionsWidget();
    void ShowMainPanel();
    void ShowOptionsPanel();
    void OnPauseOpened();
    void OnPauseClosed();

    UPROPERTY() TObjectPtr<UOptionsMenuWidget> OptionsMenuInstance = nullptr;
    bool bShowingOptions = false;
};
