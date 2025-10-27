// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CEnemyWeaponBase.h"
#include "ACTankerWeapon.generated.h"

UCLASS()
class POSISONFROG_API AACTankerWeapon : public ACEnemyWeaponBase
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AACTankerWeapon();

private:
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|State")
	float TankerWeaponDamage = 20.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|State")
	FName TankerDamageBoxSocketName = TEXT("None");

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|State")
    FVector TankerDamageBoxExtent = FVector::ZeroVector;
};
