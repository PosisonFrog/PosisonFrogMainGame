// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CUltimateBuffComponent.generated.h"


/*
	일단 기본 캐릭터 이동속도로 잡기
	이동속도 30% 증가 o
	데미지 15% 증가 o
	체력 유지 - 나중에 체력 버프 생길 수 있음 o
	방어력 관련 임의로 정의
*/

class ACPlayerCharacter;
class UCharacterMovementComponent;
class UCPlayerHealthComponent;

UENUM(BlueprintType)
enum class EUltDefenseMode : uint8
{
	None = 0,
	PercentReduction, // 경감 모드
	ArmorStat		  // 방어력 수치 -> 스케일로 환산
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class POSISONFROG_API UCUltimateBuffComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UCUltimateBuffComponent();

	UFUNCTION(BlueprintCallable, Category = "Ultimate")
	void ActivateUltimate();

	UFUNCTION(BlueprintCallable, Category = "Ultimate")
	void DeactivateUltimate();
	
	// === Weapon / Health에서 읽어갈 값 ===
	float GetOutgoingDamageMultiplier() const { return OutgoingDamageMul;}

	// HealthComponent::Damage()에서 한 줄 곱해줄 스케일
	float GetIncomingDamageScale() const { return IncomingDamageScale; }

	bool IsUltActive() const { return bIsActive; }
	
protected:
	virtual void BeginPlay() override;

private:
	void ApplyAll();
	void RestoreAll();

private:
	// 캐싱
	UPROPERTY() ACPlayerCharacter* OwnerChar = nullptr;
	UPROPERTY() UCharacterMovementComponent* MovementComponent = nullptr;
	UPROPERTY() UCPlayerHealthComponent* HealthComponent = nullptr;

	// 이동
	UPROPERTY(EditAnywhere, Category = "Ultimate|Buff", meta = (ClampMin = "0.01"))
	float MoveSpeedMul = 1.30f;
	
	float BaseMaxWalkSpeed = 0.0f;

	// 데미지
	UPROPERTY(EditAnywhere, Category = "Ultimate|Buff", meta = (ClampMin = "0.01"))
	float DamageMul = 1.15f;
	
	float DefaultDamageMultiplier = 1.0f;
	float OutgoingDamageMul = 1.0f;

	// 체력
	UPROPERTY(EditAnywhere, Category = "Ultimate|Buff", meta = (ClampMin = "0.01"))
	float MaxHpMul = 1.20f;
	
	UPROPERTY(EditAnywhere, Category = "Ultimate|Buff", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HealPercent = 0.25f; // 궁극기 사용시 체력 일정 회복값 0~1 (0.25 = 25%)
	
	float BaseMaxHealth = 0.0f;
	
	UPROPERTY(EditAnywhere, Category = "Ultimate|Buff")
	bool bHealOnActivate = false;
	
	// 방어력
	UPROPERTY(EditAnywhere, Category = "Ultimate|Defense")
	EUltDefenseMode DefenseMode = EUltDefenseMode::PercentReduction;

	UPROPERTY(EditAnywhere, Category = "Ultimate|Defense", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DamageReduction01 = 0.20f; // 20% 경감 모드일 때 사용

	UPROPERTY(EditAnywhere, Category = "Ultimate|Defense", meta = (ClampMin = "0.0"))
	float ArmorValue = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Ultimate|Defense", meta = (ClampMin = "0.0"))
	float ArmorK = 100.0f; // 방어력 스케일링 상수 (데미지 / (1 + Armor / K)

	UPROPERTY(EditAnywhere, Category = "Ultimate|Defense")
	float MinDamageScale = 0.01f; // 최소 데미지 배율
	
	float IncomingDamageScale = 1.0f;

	bool bIsActive = false;
};
