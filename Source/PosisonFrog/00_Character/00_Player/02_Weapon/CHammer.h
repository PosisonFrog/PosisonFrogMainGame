// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "00_Character/CWeaponBase.h"
#include "GameFramework/Actor.h"
#include "CHammer.generated.h"


UCLASS()
class POSISONFROG_API ACHammer : public ACWeaponBase
{
	GENERATED_BODY()
	
public:	
	ACHammer();

	FORCEINLINE USkeletalMeshComponent* GetHammerMesh() const { return GetWeaponMesh(); }
	
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void SetupDamageBox() override;
	virtual bool ShouldHitActor(AActor* OtherActor) const override;

private:
	UPROPERTY(EditAnywhere, Category = "Hammer|Targeting")
	FName EnemyTag = TEXT("Enemy");
};
