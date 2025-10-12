// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "00_Character/CWeaponBase.h"
#include "GameFramework/Actor.h"
#include "CHammer.generated.h"

class UNiagaraSystem;

USTRUCT()
struct FComboVFX_Transform
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Hammer|VFX Transform")
	FVector LocationOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Hammer|VFX Transform")
	FRotator RotationOffset = FRotator::ZeroRotator;
};

UCLASS()
class POSISONFROG_API ACHammer : public ACWeaponBase
{
	GENERATED_BODY()
	
public:	
	ACHammer();

	void PlayAttackVFX(int32 ComboIndex);
	
	FORCEINLINE USkeletalMeshComponent* GetHammerMesh() const { return GetWeaponMesh(); }
	
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void SetupDamageBox() override;
	virtual bool ShouldHitActor(AActor* OtherActor) const override;

private:
	UPROPERTY(EditAnywhere, Category = "Hammer|Targeting")
	FName EnemyTag = TEXT("Enemy");

	UPROPERTY(EditAnywhere, Category = "Hammer|VFX")
	TArray<UNiagaraSystem*> AttackVFX;

	UPROPERTY(EditAnywhere, Category = "Hammer|VFX", meta = (TitleProperty = "LocationOffset"))
	TArray<FComboVFX_Transform> AttackVFX_Transforms;
};
