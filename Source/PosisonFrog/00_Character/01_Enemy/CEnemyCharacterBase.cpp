// Fill out your copyright notice in the Description page of Project Settings.


#include "00_Character/01_Enemy/CEnemyCharacterBase.h"

ACEnemyCharacterBase::ACEnemyCharacterBase()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ACEnemyCharacterBase::BeginPlay()
{
	Super::BeginPlay();
	
	CurrentHealth = MaxHealth;
}

float ACEnemyCharacterBase::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
	class AController* EventInstigator, AActor* DamageCauser)
{
	if (DamageAmount <= 0.0f)
		return DamageAmount;

	CurrentHealth -= DamageAmount;

	if (CurrentHealth <= 0.0f)
	{
		bIsDead = true;
		OnDeath();
	}
	
	return DamageAmount;
}

void ACEnemyCharacterBase::OnDeath()
{
	if (!bIsDead)
		return;
	
	Destroy();
}
