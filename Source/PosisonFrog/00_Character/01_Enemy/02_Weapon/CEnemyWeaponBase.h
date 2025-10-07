// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "00_Character/CWeaponBase.h"
#include "CEnemyWeaponBase.generated.h"

/*
 * 적 무기 베이스
 * - 적 AI 전용 공통 기능
 */
UCLASS()
class POSISONFROG_API ACEnemyWeaponBase : public ACWeaponBase
{
	GENERATED_BODY()

public:
	ACEnemyWeaponBase();

protected:
	virtual void BeginPlay() override;
	virtual bool ShouldHitActor(AActor* OtherActor) const override;
	
public:
	UPROPERTY(EditAnywhere, Category = "PF|Enemy|Targeting")
	FName PlayerTag = TEXT("Player");
};
