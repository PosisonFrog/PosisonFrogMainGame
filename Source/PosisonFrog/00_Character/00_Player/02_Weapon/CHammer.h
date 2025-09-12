// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CHammer.generated.h"

class USkeletalMeshComponent;
class UBoxComponent;

// InstigatorActor(공격자), HitActor(피격자), Damage(수치), HitInfo(충돌 정보)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnHammerHit, AActor*, InstigatorActor, AActor*, HitActor, float, Damage, FHitResult, HitInfo);

UCLASS()
class POSISONFROG_API ACHammer : public AActor
{
	GENERATED_BODY()
	
public:	
	ACHammer();

protected:
	virtual void BeginPlay() override;
	
private:
	UPROPERTY(VisibleAnywhere)
	USkeletalMeshComponent* HammerMesh;

	UPROPERTY(VisibleAnywhere)
	UBoxComponent* DamageBox;

	UPROPERTY(VisibleAnywhere, Category = "Weapon|Damage")
	float Damage = 20.0f;

	UPROPERTY(VisibleAnywhere, Category = "Weapon|Damage")
	FName EnemyTagName = FName(TEXT("Enemy"));
	
	// 스윙 동안 맞은 액터가 중복으로 맞는걸 막기 위함
	TSet<TWeakObjectPtr<AActor>> HitActors;
	
	bool bDamageActive = false;
	
	UFUNCTION()
	void OnDamageBoxBeginOverlap(UPrimitiveComponent* OverlapComponent, AActor* OtherActor,
								 UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
								 bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void ApplyDamageHandler(AActor* InstigatorActor, AActor* HitActor, float InDamage, FHitResult HitInfo);
	
	void ResetHitActors();

public:
	// 외부에서 구독할 이벤트 (피격 알림)
	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FOnHammerHit OnHammerHit;
	
	UFUNCTION(BlueprintCallable, Category="Weapon")
	void ActivateDamage();

	UFUNCTION(BlueprintCallable, Category="Weapon")
	void DeactivateDamage();
};
