// Fill out your copyright notice in the Description page of Project Settings.


#include "00_Character/02_Component/CBaseMovementBuffComponent.h"

UCBaseMovementBuffComponent::UCBaseMovementBuffComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}


void UCBaseMovementBuffComponent::BeginPlay()
{
	Super::BeginPlay();
}
