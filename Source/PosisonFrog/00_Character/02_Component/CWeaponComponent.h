// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CWeaponComponent.generated.h"

class ACPlayerCharacter;
class ACHammer;
class UBoxComponent;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class POSISONFROG_API UCWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// 생성자
	UCWeaponComponent();

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	ACPlayerCharacter* OwnerCharacter = nullptr;
	
	UPROPERTY()
	ACHammer* Hammer = nullptr;

	// === 콤보 관련 ===
	FTimerHandle ComboResetTimer;
	int32 CurrentCombo = 0;
	bool bCanNextCombo = false;
	bool bIsAttacking = false;

	void PlayComboAttack();
	void ResetCombo();

	// === 무기 관련 ===
	void SpawnWeapon();                      // 무기 생성
	void AttachWeaponToCharacter();          // 캐릭터에 무기 부착

protected:
	// === 콤보 관련 ===
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	TArray<class UAnimMontage*> ComboMontages;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	float ComboResetTime = 1.5f;

	// === 무기 관련 ===
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TSubclassOf<ACHammer> HammerClass;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	FName AttachSocketName = FName(TEXT("Hand_Hammer"));

public:
	UFUNCTION(BlueprintCallable, Category = "Attack")
	void DoAttack();

	// === Animation Notify에서 호출되는 함수들 ===
	void BeginAction();
	void EndAction();
	
	void EnableComboInput();
	void DisableComboInput();
	
	UFUNCTION(BlueprintCallable, Category = "Attack")
	void EnableAttackBoxCollider();

	UFUNCTION(BlueprintCallable, Category = "Attack")
	void DisableAttackBoxCollider();
};
