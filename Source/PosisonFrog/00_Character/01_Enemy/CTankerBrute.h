// Source/PosisonFrog/00_Character/01_Enemy/CTankerBrute.h
#pragma once

#include "CoreMinimal.h"
#include "00_Character/01_Enemy/CEnemyCharacterBase.h"
#include "CTankerBrute.generated.h"

class UCTankerChargeComponent;

UCLASS()
class POSISONFROG_API ACTankerBrute : public ACEnemyCharacterBase
{
	GENERATED_BODY()

public:
	ACTankerBrute();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// 상위 FSM 훅
	virtual void DoChase() override;
	virtual void DoAttack() override; // (선택) 근접 기본 공격

	UFUNCTION() void HandleChargeStateChanged(EChargeState NewState, EChargeState PrevState);
	UFUNCTION() void HandleChargeFinished(EChargeEndReason Reason, AActor* Hit);

private:
	UPROPERTY(VisibleAnywhere, Category="PF|Component")
	UCTankerChargeComponent* ChargeComp = nullptr;

	// 돌진을 우선 시도할지
	UPROPERTY(EditDefaultsOnly, Category="PF|Charge")
	bool bPreferCharge = true;
	// 돌진 후 복귀 허용 거리/시간 여유
    UPROPERTY(EditDefaultsOnly, Category="PF|Charge", meta=(ClampMin="0"))
	float PostChargeChaseGraceTime = 2.5f;
	
	UPROPERTY(EditDefaultsOnly, Category="PF|Charge", meta=(ClampMin="0"))
	float ChargeStopDistanceOverride = 4500.f;
	
	float ChargeStopOverrideRestoreTime = -1.f;
	float CachedChaseStopDistance      = -1.f;
	float LastChargeFinishedTime       = -1000.f;
	
	// 기본 근접 공격 거리(옵션)
	UPROPERTY(EditDefaultsOnly, Category="PF|Melee", meta=(ClampMin="0"))
	float MeleeAttackDistance = 220.f;
};
