// Fill out your copyright notice in the Description page of Project Settings.


#include "02_MainMenu/01_Widget/CMainMenuWidget.h"

#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

void UCMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (MainMenu_StartButton)
	{
		MainMenu_StartButton->OnClicked.AddDynamic(this, &UCMainMenuWidget::MainMenu_StartButtonReleasedHandle);
	}

	if (MainMenu_SettingButton)
	{
		MainMenu_SettingButton->OnClicked.AddDynamic(this, &UCMainMenuWidget::MainMenu_SettingButtonReleasedHandle);
	}

	if (MainMenu_ExitButton)
	{
		MainMenu_ExitButton->OnClicked.AddDynamic(this, &UCMainMenuWidget::MainMenu_ExitButtonReleasedHandle);
	}
}

// ==== UI 바인드 ====
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

// ==== 애니메이션 ====
void UCMainMenuWidget::OnAnyButtonHovered()
{
	if (Anim_Hover)
	{
		PlayUISound(SFX_Hover);
		PlayAnimation(Anim_Hover);
	}
}

void UCMainMenuWidget::OnAnyButtonPressed()
{
	if (Anim_Click)
	{
		PlayUISound(SFX_Click);
		PlayAnimation(Anim_Click, 0.1f, 1, EUMGSequencePlayMode::Forward, 1.0f);
	}
}

void UCMainMenuWidget::PlayUISound(USoundBase* SFX)
{
	if (!SFX)
		return;

	if (APlayerController* PC = GetOwningPlayer())
		UGameplayStatics::PlaySound2D(PC, SFX);
}

void UCMainMenuWidget::SpawnClickVFX(FVector2D ScreenPos)
{
	
}
