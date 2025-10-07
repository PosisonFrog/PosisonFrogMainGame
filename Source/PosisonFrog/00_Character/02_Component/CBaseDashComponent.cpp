// Fill out your copyright notice in the Description page of Project Settings.


#include "00_Character/02_Component/CBaseDashComponent.h"

UCBaseDashComponent::UCBaseDashComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}


void UCBaseDashComponent::BeginPlay()
{
	Super::BeginPlay();
}
