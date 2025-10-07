// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CBaseMovementBuffComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class POSISONFROG_API UCBaseMovementBuffComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UCBaseMovementBuffComponent();

protected:
	virtual void BeginPlay() override;
};
