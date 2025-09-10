// Fill out your copyright notice in the Description page of Project Settings.


#include "02_MainMenu/01_Widget/CMainMenuWidget.h"

#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

void UCMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	MainMenu_StartButton->OnReleased.AddDynamic(this, &UCMainMenuWidget::MainMenu_StartButtonReleasedHandle);
	MainMenu_SettingButton->OnReleased.AddDynamic(this, &UCMainMenuWidget::MainMenu_SettingButtonReleasedHandle);
	MainMenu_SettingButton->OnReleased.AddDynamic(this, &UCMainMenuWidget::MainMenu_ExitButtonReleasedHandle);
}

void UCMainMenuWidget::MainMenu_StartButtonReleasedHandle()
{
	UGameplayStatics::OpenLevel(this, TEXT("ThirdPersonMap"), true);
}

void UCMainMenuWidget::MainMenu_SettingButtonReleasedHandle()
{
}

void UCMainMenuWidget::MainMenu_ExitButtonReleasedHandle()
{
	UKismetSystemLibrary::QuitGame(GetWorld(), nullptr, EQuitPreference::Quit, false);
}
