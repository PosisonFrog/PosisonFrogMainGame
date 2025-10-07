// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CBaseWeaponComponent.generated.h"

class ACWeaponBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWeaponCompHit, AActor*, HitActor, float, Damage);

/*
 * 무기 컴포넌트 베이스
 * - 무기 스폰/부착/제어
 * - 공격 로직은 자식에서 구현
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class POSISONFROG_API UCBaseWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UCBaseWeaponComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// --- 공격 인터페이스 ---
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void DoAttack() {};

	// --- 무기 제어 ---
	/** AnimNotifyState_PlayerAttack 등에서 호출 (히트창) */
	UFUNCTION(BlueprintCallable, Category = "Weapon") void EnableAttackBoxCollider();
	UFUNCTION(BlueprintCallable, Category = "Weapon") void DisableAttackBoxCollider();
	
	// --- 이벤트 ---
	UPROPERTY(BlueprintAssignable)
	FOnWeaponCompHit OnWeaponHit;

	// --- 무기 접근 ---
	ACWeaponBase* GetCurrentWeapon() const { return CurrentWeapon; }

protected:
	// --- 가상 함수 ---
	virtual void SpawnWeapon() {};
	UFUNCTION() virtual void HandleWeaponHit(AActor* InstigatorActor, AActor* HitActor, float Damage, FHitResult Hit);

	// --- 내부 헬퍼 ---
	void AttachWeaponToCharacter();
	
protected:
	// --- 소유자/무기 ---
	UPROPERTY() TWeakObjectPtr<ACharacter> OwnerChar = nullptr;
	UPROPERTY() ACWeaponBase* CurrentWeapon = nullptr;

	// --- 설정 ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TSubclassOf<ACWeaponBase> WeaponClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FName AttachSocketName = TEXT("WeaponSocket");
};
