// Fill out your copyright notice in the Description page of Project Settings.


#include "00_Character/02_Component/CHealthComponent.h"
#include "00_Character/03_AssetData/CPlayerStatAssetData.h"


void UCHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	if (PlayerStatAssetData)
	{
		MaxHealth = PlayerStatAssetData->MaxHp;
		CurrentHealth = 50.f;
	}
}

void UCHealthComponent::Healing(float InAmount)
{
	CurrentHealth += InAmount;
}

void UCHealthComponent::Damage(float InAmount)
{
	
}

/*void UCHealthComponent::BroadcastChanged()
{
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}*/
