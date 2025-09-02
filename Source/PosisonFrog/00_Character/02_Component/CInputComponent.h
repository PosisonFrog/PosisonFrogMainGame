// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"

#include "CInputComponent.generated.h"

class UEnhancedInputLocalPlayerSubsystem;
class UInputAction;
class UObject;

UCLASS(Config = Input)
class POSISONFROG_API UCInputComponent : public UEnhancedInputComponent
{
	GENERATED_BODY()

public:
	UCInputComponent();

};
