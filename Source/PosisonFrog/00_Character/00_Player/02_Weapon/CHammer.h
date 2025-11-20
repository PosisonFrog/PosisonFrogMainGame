// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "00_Character/CWeaponBase.h"
#include "GameFramework/Actor.h"
#include "CHammer.generated.h"

class UNiagaraSystem;

/*USTRUCT()
struct FComboVFX_Transform
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Hammer|VFX Transform")
	FVector LocationOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Hammer|VFX Transform")
	FRotator RotationOffset = FRotator::ZeroRotator;
};*/

UCLASS()
class POSISONFROG_API ACHammer : public ACWeaponBase
{
	GENERATED_BODY()
	
public:	
	ACHammer();

	FORCEINLINE USkeletalMeshComponent* GetHammerMesh() const { return GetWeaponMesh(); }

	// 이펙트 에셋 접근자
	UNiagaraSystem* GetHitEffect_Normal() const { return HitEffect_Normal; }
	UNiagaraSystem* GetHitEffect_Ultimate() const { return HitEffect_Ultimate; }
	FVector GetHitEffectLocationOffset() const { return HitEffectLocationOffset; }
	FRotator GetHitEffectRotationOffset() const { return HitEffectRotationOffset; }
	float GetHitEffectScale() const { return HitEffectScale; }
	
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void SetupDamageBox() override;
	virtual bool ShouldHitActor(AActor* OtherActor) const override;

private:
	// ───────── 타게팅 ─────────
	UPROPERTY(EditAnywhere, Category = "Hammer|Targeting")
	FName EnemyTag = TEXT("Enemy");

	// ───────── 타격 이펙트 ─────────
	// 일반 상태 타격 이펙트
	UPROPERTY(EditAnywhere, Category = "Hammer|Hit Effects")
	UNiagaraSystem* HitEffect_Normal = nullptr;

	// 궁극기 상태 타격 이펙트
	UPROPERTY(EditAnywhere, Category = "Hammer|Hit Effects")
	UNiagaraSystem* HitEffect_Ultimate = nullptr;

	// 이펙트 스폰 위치 오프셋 (피격 지점 기준)
	UPROPERTY(EditAnywhere, Category = "Hammer|Hit Effects")
	FVector HitEffectLocationOffset = FVector(0.f, 0.f, 50.f);

	// 이펙트 스폰 회전 오프셋
	UPROPERTY(EditAnywhere, Category = "Hammer|Hit Effects")
	FRotator HitEffectRotationOffset = FRotator::ZeroRotator;

	// 이펙트 스케일
	UPROPERTY(EditAnywhere, Category = "Hammer|Hit Effects", meta = (ClampMin = "0.1"))
	float HitEffectScale = 1.0f;
};
