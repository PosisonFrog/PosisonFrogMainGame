// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CHammer.generated.h"

class USkeletalMeshComponent;
class UBoxComponent;
class ACharacter;

// InstigatorActor(공격자), HitActor(피격자), Damage(수치), HitInfo(충돌 정보)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnHammerHit, AActor*, InstigatorActor, AActor*, HitActor, float, Damage, FHitResult, HitInfo);

UCLASS()
class POSISONFROG_API ACHammer : public AActor
{
	GENERATED_BODY()
	
public:	
	ACHammer();
	
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void ActivateDamage();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void DeactivateDamage();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void ResetHitActors();

	// 외부에서 구독할 이벤트 (피격 알림)
	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FOnHammerHit OnHammerHit;

	// AnimBP에서 호출할 수 있는 함수들 추가
	//캐싱된 플레이어 사용
	UFUNCTION(BlueprintCallable, Category = "PlayerAnimBP")
	ACharacter* GetOwnerCharacter() const;

	// 플레이어가 이동중인지 확인
	UFUNCTION(BlueprintPure, Category = "PlayerAnimBP")
	float GetOwnerSpeed() const;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnDamageBoxBeginOverlap(UPrimitiveComponent* OverlapComponent, AActor* OtherActor,
								 UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
								 bool bFromSweep, const FHitResult& SweepResult);

	//UFUNCTION()
	//void ApplyDamageHandler(AActor* InstigatorActor, AActor* HitActor, float InDamage, FHitResult HitInfo);

	bool ShouldHitActor(AActor* OtherActor) const;
	
protected:
	// ===== Components =====
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components", meta = (AllowPrivateAccess = "true"))
	USceneComponent* DefaultSceneRoot = nullptr;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* HammerMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components", meta = (AllowPrivateAccess = "true"))
	UBoxComponent* DamageBox = nullptr;
	 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hammer|Damage")
	FName DamageBoxSocketName = FName("HammerHead_Socket");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hammer|Damage")
	FVector DamageBoxRelativeLocation = FVector::ZeroVector;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hammer|Damage")
	FRotator DamageBoxRelativeRotation = FRotator::ZeroRotator;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hammer|Damage")
	FVector DamageBoxExtent = FVector(54.0f, 60.0f, 60.0f);

	// ===== Damage / Filter =====
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Damage", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float Damage = 20.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Damage", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UDamageType> DamageTypeClass;

	// 적 식별(태그)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Damage", meta = (AllowPrivateAccess = "true"))
	FName EnemyTag = TEXT("Enemy");

	// 실제 데미지를 서버에서만 처리할지
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Net")
	bool bServerOnlyDamage = true;

	// ===== State / Debug =====
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Weapon|Debug", meta = (AllowPrivateAccess = "true"))
	bool bDamageActive = false;

	UPROPERTY(EditAnywhere, Category = "Weapon|Debug")
	bool bDebugLog = false;
	
	// 중복 타격 방지를 위한 약참조
	TSet<TWeakObjectPtr<AActor>> HitActors;


public:
	// 읽기 편의 Getter
	FORCEINLINE float GetDamage() const { return Damage; }
	FORCEINLINE USkeletalMeshComponent* GetHammerMesh() const { return HammerMesh; }

private:
	UPROPERTY()
	TWeakObjectPtr<ACharacter> CachedOwnerCharacter;
};
