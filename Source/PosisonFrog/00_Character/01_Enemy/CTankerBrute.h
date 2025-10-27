// Source/PosisonFrog/00_Character/01_Enemy/CTankerBrute.h
#pragma once

#include "CoreMinimal.h"
#include "00_Character/01_Enemy/CEnemyCharacterBase.h"
#include "CTankerBrute.generated.h"

class UCTankerChargeComponent;
class UAnimMontage;
class USoundBase;
class USoundBase;
class UNiagaraSystem;


/**
 * Brute-style enemy that relies on the tanker charge component for mobility and damage.
 * The logic mirrors the previous implementation but is reorganised for clarity.
*/

UCLASS()
class POSISONFROG_API ACTankerBrute : public ACEnemyCharacterBase
{
	GENERATED_BODY()

public:
	ACTankerBrute();
	virtual void Tick(float DeltaSeconds) override;
	
protected:
	virtual void BeginPlay() override;


private:

	// 상위 FSM 훅
	virtual bool HasVisualOnTarget() const override;
	virtual void DoChase() override;
	virtual void DoAttack() override; // (선택) 근접 기본 공격
	virtual void OnDead() override;
	
	void InitialiseChargeComponent();
	bool ShouldAttemptCharge() const;
	bool TryStartCharge();
	void UpdateChargeStopOverride(float CurrentTime);
	void HandleImmediatePostCharge(float CurrentTime);
	
	UFUNCTION()
	void HandleChargeStateChanged(EChargeState NewState, EChargeState PreviousState);
	
	UFUNCTION()
	void HandleChargeFinished(EChargeEndReason Reason, AActor* HitActor);

public:
	UPROPERTY(EditAnywhere, Category="PF|Animation")
	TObjectPtr<UAnimMontage> DeadMontage = nullptr;

	UPROPERTY(EditAnywhere, Category="PF|Sound")
	TObjectPtr<USoundBase> HitSound = nullptr;

	UPROPERTY(EditAnywhere, Category="PF|Effects")
	TObjectPtr<UNiagaraSystem> HitEffect = nullptr;
	
private:
	UPROPERTY(VisibleAnywhere, Category = "PF|Component")
	TObjectPtr<UCTankerChargeComponent> ChargeComp = nullptr;
	
	UPROPERTY(EditDefaultsOnly, Category = "PF|Charge")
	bool bPreferCharge = true;
	
	UPROPERTY(EditDefaultsOnly, Category = "PF|Charge", meta = (ClampMin = "0"))
	float PostChargeChaseGraceTime = 2.5f;
	
	UPROPERTY(EditDefaultsOnly, Category = "PF|Charge", meta = (ClampMin = "0"))
	float ChargeStopDistanceOverride = 4500.f;
	
	float ChargeStopOverrideRestoreTime = -1.f;
	float CachedChaseStopDistance = -1.f;
	float LastChargeFinishedTime = -1000.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "PF|Melee", meta = (ClampMin = "0"))
	float MeleeAttackDistance = 220.f;
};
