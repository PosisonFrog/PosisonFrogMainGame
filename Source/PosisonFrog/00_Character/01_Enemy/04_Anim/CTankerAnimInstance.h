#pragma once

#include "Animation/AnimInstance.h"
#include "00_Character/02_Component/01_EnemyComponent/CTankerChargeComponent.h" // Enum 정의된 헤더
#include "CTankerAnimInstance.generated.h"

UCLASS()
class POSISONFROG_API UCTankerAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:

	UPROPERTY(BlueprintReadOnly, Category = "Charge")
	EChargeState ChargeState;
	
	// State Machine에서 사용할 Bool 변수들
	UPROPERTY(BlueprintReadOnly, Category = "Charge")
	bool bIsIdle;

	UPROPERTY(BlueprintReadOnly, Category = "Charge")
	bool bIsPreCharge;

	UPROPERTY(BlueprintReadOnly, Category = "Charge")
	bool bIsWindup;

	UPROPERTY(BlueprintReadOnly, Category = "Charge")
	bool bIsCharging;

	UPROPERTY(BlueprintReadOnly, Category = "Charge")
	bool bIsRecovery;

	UPROPERTY(BlueprintReadOnly, Category = "Charge")
	bool bIsOnCooldown;

	UPROPERTY(BlueprintReadOnly, Category = "Movement")
	float MoveSpeed;

private:
	UPROPERTY()
	TObjectPtr<UCTankerChargeComponent> ChargeComp;
	
	UPROPERTY()
	TObjectPtr<UCharacterMovementComponent> MovementComp;
	
	UPROPERTY()
	TObjectPtr<ACharacter> OwnerCharacter;
};
