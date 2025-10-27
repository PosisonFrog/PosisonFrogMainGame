#pragma once

#include "Animation/AnimInstance.h"
#include "00_Character/02_Component/01_EnemyComponent/CTankerChargeComponent.h"
#include "CTankerAnimInstance.generated.h"

UCLASS()
class POSISONFROG_API UCTankerAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	void CacheOwnerReferences();
	void UpdateChargeStateVariables();
	void UpdateMovementVariables();
	
protected:
	UPROPERTY(BlueprintReadOnly, Category = "Charge")
	EChargeState ChargeState = EChargeState::Idle;
	
	// State Machine에서 사용할 Bool 변수들
	UPROPERTY(BlueprintReadOnly, Category = "Charge")
	bool bIsIdle = true;

	UPROPERTY(BlueprintReadOnly, Category = "Charge")
	bool bIsPreCharge = false;

	UPROPERTY(BlueprintReadOnly, Category = "Charge")
	bool bIsWindup = false;

	UPROPERTY(BlueprintReadOnly, Category = "Charge")
	bool bIsCharging = false;

	UPROPERTY(BlueprintReadOnly, Category = "Charge")
	bool bIsRecovery = false;

	UPROPERTY(BlueprintReadOnly, Category = "Charge")
	bool bIsOnCooldown = false;

	UPROPERTY(BlueprintReadOnly, Category = "Movement")
	float MoveSpeed = 0.f;

private:
	TWeakObjectPtr<UCTankerChargeComponent> ChargeComponent;
	TWeakObjectPtr<UCharacterMovementComponent> MovementComponent;
	TWeakObjectPtr<ACharacter> OwnerCharacter;
};
