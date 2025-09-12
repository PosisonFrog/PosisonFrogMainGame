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
	else
	{
		// PlayerStatAssetData가 없는 경우를 대비한 기본값 설정
		MaxHealth = 100.f;
		CurrentHealth = MaxHealth;
	}
	
	if (OnHealthChanged.IsBound())
	{
		OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
	}
}

void UCHealthComponent::Healing(float InAmount)
{
	CurrentHealth = FMath::Clamp(CurrentHealth + InAmount, 0.0f, MaxHealth);
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}

void UCHealthComponent::Damage(float InAmount)
{
	CurrentHealth -= InAmount;
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}
