// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CMainMenuController.generated.h"

class UCMainMenuWidget;
/**
 * 
 */
UCLASS()
class POSISONFROG_API ACMainMenuController : public APlayerController
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UCMainMenuWidget> MainMenuWidgetClass;

	UPROPERTY()
	UCMainMenuWidget* MainMenuWidget = nullptr;
	
public:
	ACMainMenuController();

	virtual void BeginPlay() override;
};
