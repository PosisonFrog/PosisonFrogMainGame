// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CEnemyWeaponBase.h"
#include "ACRiotWeapon.generated.h"

UCLASS()
class POSISONFROG_API AACRiotWeapon : public ACEnemyWeaponBase
{
	GENERATED_BODY()

public:
	AACRiotWeapon();

private:
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|State")
	float RiotWeaponDamage = 15.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|State")
	FName RiotDamageBoxSocketName = TEXT("None");

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|State")
	FVector RiotDamageBoxExtent = FVector(0.0f, 0.0f, 0.0f);
	
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|State")
	FVector RiotDamageBoxRelativeLocation = FVector(0.0f, 0.0f, 0.0f);
};
