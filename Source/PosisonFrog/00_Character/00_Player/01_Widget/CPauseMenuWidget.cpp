// Fill out your copyright notice in the Description page of Project Settings.


#include "CPauseMenuWidget.h"

#include "02_MainMenu/01_Widget/OptionsMenuWidget.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"

void UPauseMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Button_Resume)
    {
        Button_Resume->OnClicked.AddDynamic(this, &UPauseMenuWidget::HandleResumeClicked);
    }
    
    if (Button_Settings)
    {
        Button_Settings->OnClicked.AddDynamic(this, &UPauseMenuWidget::HandleSettingsClicked);
    }

    if (Button_Restart)
    {
        Button_Restart->OnClicked.AddDynamic(this, &UPauseMenuWidget::HandleRestartClicked);
    }

    if (Button_ReturnToTitle)
    {
        Button_ReturnToTitle->OnClicked.AddDynamic(this, &UPauseMenuWidget::HandleReturnToTitleClicked);
    }

    if (Button_Exit)
    {
        Button_Exit->OnClicked.AddDynamic(this, &UPauseMenuWidget::HandleExitClicked);
    }

    if (Button_BackFromSettings)
    {
        Button_BackFromSettings->OnClicked.AddDynamic(this, &UPauseMenuWidget::HandleBackFromSettingsClicked);
    }

    ResetMenuState();
}

void UPauseMenuWidget::NativeDestruct()
{
    if (OptionsMenuInstance)
    {
        OptionsMenuInstance->OnClosed.RemoveAll(this);
        OptionsMenuInstance->RemoveFromParent();
        OptionsMenuInstance = nullptr;
    }

    Super::NativeDestruct();
}

FReply UPauseMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    const FKey PressedKey = InKeyEvent.GetKey();

    if (bShowingOptions)
    {
        if (PressedKey == EKeys::Escape || PressedKey == EKeys::Gamepad_Special_Right || PressedKey == EKeys::Gamepad_FaceButton_Right)
        {
            HandleBackFromSettingsClicked();
            return FReply::Handled();
        }
    }

    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UPauseMenuWidget::FocusInitial()
{
    if (Button_Resume)
    {
        Button_Resume->SetKeyboardFocus();
        return;
    }
    
    if (Button_Settings)
    {
        Button_Settings->SetKeyboardFocus();
        return;
    }

    if (Button_Restart)
    {
        Button_Restart->SetKeyboardFocus();
        return;
    }

    if (Button_ReturnToTitle)
    {
        Button_ReturnToTitle->SetKeyboardFocus();
        return;
    }

    if (Button_Exit)
    {
        Button_Exit->SetKeyboardFocus();
    }
}

void UPauseMenuWidget::ResetMenuState()
{
    ShowMainPanel();
}

void UPauseMenuWidget::HandleResumeClicked()
{
    OnResumeRequested.Broadcast();
}

void UPauseMenuWidget::HandleSettingsClicked()
{
    EnsureOptionsWidget();

    if (OptionsMenuInstance)
    {
        ShowOptionsPanel();
        OptionsMenuInstance->FocusInitial();
    }
}

void UPauseMenuWidget::HandleRestartClicked()
{
    OnRestartRequested.Broadcast();
}

void UPauseMenuWidget::HandleReturnToTitleClicked()
{
    OnReturnToTitleRequested.Broadcast();
}

void UPauseMenuWidget::HandleExitClicked()
{
    OnExitRequested.Broadcast();
}

void UPauseMenuWidget::HandleBackFromSettingsClicked()
{
    HandleOptionsClosed();
}

void UPauseMenuWidget::HandleOptionsClosed()
{
    ShowMainPanel();
    FocusInitial();
}

void UPauseMenuWidget::EnsureOptionsWidget()
{
    if (!OptionsMenuInstance && OptionsMenuClass)
    {
        if (APlayerController* PC = GetOwningPlayer())
        {
            OptionsMenuInstance = CreateWidget<UOptionsMenuWidget>(PC, OptionsMenuClass);
            if (OptionsMenuInstance)
            {
                OptionsMenuInstance->OnClosed.AddDynamic(this, &UPauseMenuWidget::HandleOptionsClosed);
            }
        }
    }

    if (OptionsMenuInstance)
    {
        if (OptionsContainer)
        {
            if (OptionsMenuInstance->GetParent() != OptionsContainer)
            {
                OptionsMenuInstance->RemoveFromParent();
                OptionsContainer->AddChild(OptionsMenuInstance);
            }
        }
        else if (!OptionsMenuInstance->IsInViewport())
        {
            OptionsMenuInstance->AddToViewport(1100);
        }
    }
}

void UPauseMenuWidget::ShowMainPanel()
{
    if (MainPanel)
    {
        MainPanel->SetVisibility(ESlateVisibility::Visible);
    }

    if (OptionsPanel)
    {
        OptionsPanel->SetVisibility(ESlateVisibility::Collapsed);
    }

    if (OptionsMenuInstance)
    {
        OptionsMenuInstance->SetVisibility(ESlateVisibility::Collapsed);
    }

    bShowingOptions = false;
}

void UPauseMenuWidget::ShowOptionsPanel()
{
    if (MainPanel)
    {
        MainPanel->SetVisibility(ESlateVisibility::Collapsed);
    }

    if (OptionsPanel)
    {
        OptionsPanel->SetVisibility(ESlateVisibility::Visible);
    }

    if (OptionsMenuInstance)
    {
        OptionsMenuInstance->SetVisibility(ESlateVisibility::Visible);
    }

    bShowingOptions = true;
}
