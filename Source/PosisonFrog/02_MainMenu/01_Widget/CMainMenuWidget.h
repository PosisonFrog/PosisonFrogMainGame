// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CMainMenuWidget.generated.h"

class UButton;
/**
 * 
 */
UCLASS()
class POSISONFROG_API UCMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UButton> MainMenu_StartButton;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UButton> MainMenu_SettingButton;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UButton> MainMenu_ExitButton;

protected:
	UFUNCTION()
	void MainMenu_StartButtonReleasedHandle();

	UFUNCTION()
	void MainMenu_SettingButtonReleasedHandle();

	UFUNCTION()
	void MainMenu_ExitButtonReleasedHandle();
};
