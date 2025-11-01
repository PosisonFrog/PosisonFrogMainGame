// Source/PosisonFrog/00_Character/01_Enemy/CTankerBrute.h
#pragma once

#include "CoreMinimal.h"
#include "00_Character/01_Enemy/CEnemyCharacterBase.h"
#include "CTankerBrute.generated.h"

class UCTankerChargeComponent;
class AAIController;
class UAnimMontage;
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

	virtual void PostInitProperties() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	
	virtual void BeginPlay() override;


private:

	// 상위 FSM 훅
	virtual bool HasVisualOnTarget() const override;
	virtual void DoChase() override;
	virtual void DoAttack() override; // (선택) 근접 기본 공격
	virtual void OnDead() override;
	virtual void ExitState(EEnemyState OldState) override; // Attack 상태 종료 시 타이머 정리

	// 선택(기본 공격) 흐름
	void StartAttack();               // 공격 시작(윈드업 타이머 시작)
	void BeginAttackWindow();         // 스윙 창 오픈(지속 스윕 + 즉시 1회 판정)
	void EndAttackWindow(bool bForced = true); // 스윙 창 종료
	void FinishAttack();              // 리커버리 종료 → 공격 종료 처리
	void CancelAttack();   

	// 공격 핼퍼
	AAIController* GetEnemyAIController() const;
	void StopMovement() const;
	void StopMovementAndFaceTarget();
	void PlayMontageIfValid(UAnimMontage* Montage, float PlayRate = 1.f) const;
	void PlaySoundIfValid(USoundBase* Sound) const;
	void SpawnAttackEffect() const;
	void SpawnHitEffectAtForward() const;
	void SpawnHitEffectAtLocation() const;
	void ClearAttackTimers();
	void SyncAttackTuning();
	
	void InitialiseChargeComponent();
	bool ShouldAttemptCharge() const;
	bool TryStartCharge();
	void UpdateChargeStopOverride(float CurrentTime);
	void HandleImmediatePostCharge(float CurrentTime);
	
	UFUNCTION()
	void HandleChargeStateChanged(EChargeState NewState, EChargeState PreviousState);
	
	UFUNCTION()
	void HandleChargeFinished(EChargeEndReason Reason, AActor* HitActor);

protected:
	// ───────── 기본 공격 설정(튜닝) ─────────
	
	/** 공격 주기(쿨다운) */
	UPROPERTY(EditAnywhere, Category="PF|Attack", meta=(ClampMin="0.1", ClampMax="5.0"))
	float AttackIntervalTanker = 3.0f;

	/** 공격 유효 거리(안전망) */
	UPROPERTY(EditAnywhere, Category="PF|Attack")
	float AttackRangeTanker = 300.f;

	/** 1. 윈드업(예비동작) 시간 */
	UPROPERTY(EditAnywhere, Category="PF|Attack")
	float AttackWindUpTime = 0.35f;
	
	/** 2. 스윙 창 지속 시간(분할 스윕 반복) */
	UPROPERTY(EditAnywhere, Category="PF|Attack")
	float AttackActiveWindow = 0.94f;

	/** 3. 리커버리(후딜) 시간 */
	UPROPERTY(EditAnywhere, Category="PF|Attack")
	float AttackRecoveryTime = 0.95f;

	/** 일격 데미지 (BaseDamage와 동기화) */
	UPROPERTY(EditAnywhere, Category="PF|Attack")
	float AttackDamage =  8.5f;

	/** 일격 데미지 (BaseDamage와 동기화) */
	UPROPERTY(EditAnywhere, Category="PF|Attack")
	float ChargeAttackFollowUpWindow =  2.0f;

	
	// ───────── 연출 ─────────
	UPROPERTY(EditAnywhere, Category="PF|Animation")
	UAnimMontage* AttackMontage = nullptr;
	
	UPROPERTY(EditAnywhere, Category="PF|Animation")
	TObjectPtr<UAnimMontage> DeadMontage = nullptr;

	UPROPERTY(EditAnywhere, Category="PF|Sound")
	USoundBase* AttackSound = nullptr;

	UPROPERTY(EditAnywhere, Category="PF|Effects")
	UNiagaraSystem* AttackEffect = nullptr;
	
	UPROPERTY(EditAnywhere, Category="PF|Sound")
	TObjectPtr<USoundBase> HitSound = nullptr;

	UPROPERTY(EditAnywhere, Category="PF|Effects")
	TObjectPtr<UNiagaraSystem> HitEffect = nullptr;

	
	
	// 타이머
	FTimerHandle Timer_WindUp;
	FTimerHandle Timer_EndWindow;
	FTimerHandle Timer_Finish;
	
	UPROPERTY(VisibleAnywhere, Category = "PF|Component")
	TObjectPtr<UCTankerChargeComponent> ChargeComp = nullptr;
	
	UPROPERTY(EditDefaultsOnly, Category = "PF|Charge")
	bool bPreferCharge = true;
	
	UPROPERTY(EditDefaultsOnly, Category = "PF|Charge", meta = (ClampMin = "0"))
	float PostChargeChaseGraceTime = 2.5f;
	
	UPROPERTY(EditDefaultsOnly, Category = "PF|Charge", meta = (ClampMin = "0"))
	float ChargeStopDistanceOverride = 4500.f;

	UPROPERTY(EditDefaultsOnly, Category = "PF|Melee", meta = (ClampMin = "0"))
	float MeleeAttackDistance = 220.f;

	// 디버그
	UPROPERTY(EditAnywhere, Category="PF|Debug")
	bool bDebugAttackLog = false;
	
private:
	float ChargeStopOverrideRestoreTime = -1.f;
	float CachedChaseStopDistance = -1.f;
	float LastChargeFinishedTime = -1000.f;

	//기본 공격
	bool bIsAttacking = false;
	float AttackStartedTime = 0.f;

	
	

};


