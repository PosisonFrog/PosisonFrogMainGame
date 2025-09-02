// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CWeaponComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class POSISONFROG_API UCWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// 생성자
	UCWeaponComponent();

protected:
	virtual void BeginPlay() override;

public:
	// 오버라이드된 UActorComponent 함수
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Attack")
	void DoAttack();

private:
	UPROPERTY()
	class ACharacter* OwnerCharacter;

	// === 콤보 관련 ===
	UPROPERTY(EditAnywhere, Category = "Attack")
	TArray<UAnimMontage*> ComboMontages;

	UPROPERTY(EditAnywhere, Category = "Attack")
	float ComboResetTime = 1.5f;
	FTimerHandle ComboResetTimer;
	
	int32 CurrentCombo = 0;
	bool bCanNextCombo = false;
	bool bIsAttacking = false;

	void PlayComboAttack();
	void ResetCombo();

public:
	void BeginAction();
	void EndAction();
	
	void EnableComboInput();
	void DisableComboInput();
};
