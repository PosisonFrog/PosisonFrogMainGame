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

	// --- 무기 회전 제어 (애니메이션 루트본 회전값 세팅이 다른 경우가 생겼음.)---
	/** 무기에 추가 회전 적용 */
	void RotateWeapon(const FRotator& AdditionalRotation);
	
	/** 무기 회전을 원래대로 복구 */
	void RestoreWeaponRotation();
	
protected:
	// --- 소유자/무기 ---
	UPROPERTY() TWeakObjectPtr<ACharacter> OwnerChar = nullptr;
	UPROPERTY() ACWeaponBase* CurrentWeapon = nullptr;

	// --- 설정 ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TSubclassOf<ACWeaponBase> WeaponClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FName AttachSocketName = TEXT("WeaponSocket");

protected:
	// ----원래 회전값 저장----
	FRotator OriginalWeaponRotation;
	bool bWeaponRotationModified = false;

	
};
