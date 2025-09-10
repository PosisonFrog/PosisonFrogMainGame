// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CPlayerController.generated.h"

class UInputMappingContext;
class UCInputConfig;
/**
 * 
 */
UCLASS()
class POSISONFROG_API ACPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ACPlayerController();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UCInputConfig* InputConfig = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputMappingContext* DefaultMappingContext = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	bool bClearPreviousMappings = true;
};
