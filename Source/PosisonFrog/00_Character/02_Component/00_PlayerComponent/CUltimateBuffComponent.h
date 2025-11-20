#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CUltimateBuffComponent.generated.h"

class UCPlayerMovementBuffComponent;
class ACPlayerCharacter;
class UCharacterMovementComponent;
class UCPlayerHealthComponent;
class UPostProcessComponent; // APostProcessVolume 대신

UENUM(BlueprintType)
enum class EUltDefenseMode : uint8
{
	None = 0,
	PercentReduction,
	ArmorStat
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
	
	float GetOutgoingDamageMultiplier() const { return OutgoingDamageMul;}
	float GetIncomingDamageScale() const { return IncomingDamageScale; }
	bool IsUltActive() const { return bIsActive; }
	
protected:
	virtual void BeginPlay() override;

private:
	void ApplyAll();
	void RestoreAll();
	void EnablePostProcess();
	void DisablePostProcess();

private:
	UPROPERTY() ACPlayerCharacter* OwnerChar = nullptr;
	UPROPERTY() UCharacterMovementComponent* MovementComponent = nullptr;
	UPROPERTY() UCPlayerHealthComponent* HealthComponent = nullptr;
	UPROPERTY() UCPlayerMovementBuffComponent* MovementBuffComponent = nullptr;

	// ───────── 포스트 프로세스 ─────────
	UPROPERTY() UPostProcessComponent* PostProcessComponent = nullptr;

	UPROPERTY(EditAnywhere, Category = "Ultimate|Visual Effects")
	UMaterialInterface* GlitchMaterial = nullptr; // 머티리얼 에셋 참조
	
	UPROPERTY(EditAnywhere, Category = "Ultimate|Visual Effects")
	float PostProcessBlendRadius = 0.0f; // 0 = 전체 영향

	// 시작부터 궁극기 활성화
	UPROPERTY(EditAnywhere, Category = "Ultimate|Debug", meta = (AllowPrivateAccess = "true"))
	bool bStartWithUltimateActive = false;

	// ───────── 이동 ─────────
	UPROPERTY(EditAnywhere, Category = "Ultimate|Buff", meta = (ClampMin = "0.01"))
	float MoveSpeedMul = 1.30f;
	float BaseMaxWalkSpeed = 0.0f;

	// ───────── 데미지 ─────────
	UPROPERTY(EditAnywhere, Category = "Ultimate|Buff", meta = (ClampMin = "0.01"))
	float DamageMul = 1.15f;
	float DefaultDamageMultiplier = 1.0f;
	float OutgoingDamageMul = 1.0f;

	// ───────── 체력 ─────────
	UPROPERTY(EditAnywhere, Category = "Ultimate|Buff", meta = (ClampMin = "0.01"))
	float MaxHpMul = 1.20f;
	UPROPERTY(EditAnywhere, Category = "Ultimate|Buff", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HealPercent = 0.25f;
	float BaseMaxHealth = 0.0f;
	UPROPERTY(EditAnywhere, Category = "Ultimate|Buff")
	bool bHealOnActivate = false;
	
	// ───────── 방어력 ─────────
	UPROPERTY(EditAnywhere, Category = "Ultimate|Defense")
	EUltDefenseMode DefenseMode = EUltDefenseMode::PercentReduction;
	UPROPERTY(EditAnywhere, Category = "Ultimate|Defense", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DamageReduction01 = 0.20f;
	UPROPERTY(EditAnywhere, Category = "Ultimate|Defense", meta = (ClampMin = "0.0"))
	float ArmorValue = 0.0f;
	UPROPERTY(EditAnywhere, Category = "Ultimate|Defense", meta = (ClampMin = "0.0"))
	float ArmorK = 100.0f;
	UPROPERTY(EditAnywhere, Category = "Ultimate|Defense")
	float MinDamageScale = 0.01f;
	float IncomingDamageScale = 1.0f;

	bool bIsActive = false;
};