// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CHealthComponent.generated.h"

class UCPlayerStatAssetData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChanged, float, CurrentHealth, float, MaxHealth);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class POSISONFROG_API UCHealthComponent : public UActorComponent
{
private:
	GENERATED_BODY()

	float CurrentHealth = 0.0f;
	float MaxHealth = 100.0f;
	
protected:
	UPROPERTY(EditDefaultsOnly, Category = "Stat")
	TObjectPtr<UCPlayerStatAssetData> PlayerStatAssetData;

	// Called when the game starts
	virtual void BeginPlay() override;

public:
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnHealthChanged OnHealthChanged;

	FORCEINLINE bool IsDead() const { return CurrentHealth <= 0.0f; }
	FORCEINLINE float GetHealth() const { return CurrentHealth; }
	FORCEINLINE float GetMaxHealth() const { return MaxHealth; }

	void Healing(float InAmount);
	void Damage(float InAmount);
};
