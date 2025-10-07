// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CWeaponBase.generated.h"

class UBoxComponent;
// 무기가 적중했을 때 브로드캐스트 (모든 무기 공통)
// InstigatorActor(공격자), HitActor(피격자), Damage(수치), HitInfo(충돌 정보)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnWeaponBaseHit, AActor*, InstigatorActor, AActor*, HitActor, float, Damage, FHitResult, HitInfo);

/*
 * 무기 액터 베이스
 * - 모든 무기의 공통 인터페이스
 * - 히트 감지 및 브로드캐스트
 * - 실제 데미지 적용은 WeaponComponent에서 처리
 */
UCLASS()
class POSISONFROG_API ACWeaponBase : public AActor
{
	GENERATED_BODY()

public:
	ACWeaponBase();

	// --- 무기 제어 ---
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void ActivateDamage();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void DeactivateDamage();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void ResetHitActors();

	// --- 이벤트 ---
	// 외부에서 구독할 피격 알림
	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FOnWeaponBaseHit OnWeaponBaseHit;

	// --- 접근자 ---
	float GetDamage() const { return Damage; }
	USkeletalMeshComponent* GetWeaponMesh() const { return WeaponMesh; }
	ACharacter* GetCachedOwnerCharacter() const { return CachedOwnerCharacter.Get(); }

	// 플레이어가 이동중인지 확인
	UFUNCTION(BlueprintCallable, Category = "Weapon") float GetOwnerSpeed() const;
	
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void SetupDamageBox();
	virtual bool ShouldHitActor(AActor* OtherActor) const;

	UFUNCTION()
	virtual void OnDamageBoxBeginOverlap(UPrimitiveComponent* OverlapComponent, AActor* OtherActor,
								 UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
								 bool bFromSweep, const FHitResult& SweepResult);
	
protected:
	// --- 컴포넌트 ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components", meta = (AllowPrivateAccess = "true"))
	USceneComponent* DefaultSceneRoot = nullptr;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* WeaponMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components", meta = (AllowPrivateAccess = "true"))
	UBoxComponent* DamageBox = nullptr;

	// --- 설정 ---
	// DamageBox 설정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|DamageBox")
	FName DamageBoxSocketName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|DamageBox")
	FVector DamageBoxRelativeLocation = FVector::ZeroVector;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|DamageBox")
	FRotator DamageBoxRelativeRotation = FRotator::ZeroRotator;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|DamageBox")
	FVector DamageBoxExtent = FVector(50.0f, 50.0f, 50.0f);
	
	// Damage
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|DamageBox", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float Damage = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Damage", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UDamageType> DamageTypeClass;

	// 실제 데미지를 서버에서만 처리할지
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Net")
	bool bServerOnlyDamage = true;
	
	// --- 디버그 ---
	UPROPERTY(EditAnywhere, Category = "Weapon|Debug")
	bool bDebugLog = false;

	// --- 상태 ---
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Weapon|Debug", meta = (AllowPrivateAccess = "true"))
	bool bDamageActive = false;

	// 중복 타격 방지를 위한 약참조
	TSet<TWeakObjectPtr<AActor>> HitActors;

	UPROPERTY()
	TWeakObjectPtr<ACharacter> CachedOwnerCharacter;
};
