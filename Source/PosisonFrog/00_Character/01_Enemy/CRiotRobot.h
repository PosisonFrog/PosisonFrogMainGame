#pragma once
#include "CoreMinimal.h"
#include "00_Character/01_Enemy/CEnemyCharacterBase.h"
#include "CRiotRobot.generated.h"

class UAnimMontage;
class USoundBase;
class UNiagaraSystem;
class AAIController;
class ACTacticalEnemyAIController;

/**
 * 일반형: 진압 로봇 [Riot Robot]
 * - 근접 1타 위주의 근접형
 * - 전술 패턴: 링 포위(ChaseRing) / 스트레이프(Strafe) / 재배치(Reposition)
 * - 공격은 C++ 타이머 기반 스윙 창(AttackWindow)으로 수행 (블루프린트 노티파이 없이도 동작)
 */
UCLASS()
class POSISONFROG_API ACRiotRobot : public ACEnemyCharacterBase
{
    GENERATED_BODY()
public:
    ACRiotRobot();

protected:
    // AActor
    virtual void BeginPlay() override;

    // ACEnemyCharacterBase
    virtual void DoChase() override;   // 전술 컨트롤러와 연동
    virtual void DoAttack() override;  // 타이머 기반 스윙 창
    virtual void OnDead() override;
    virtual void ExitState(EEnemyState OldState) override; // Attack 상태 종료 시 타이머 정리

    // ───────── 공격 흐름 ─────────
    void StartAttack();               // 공격 시작(윈드업 타이머 시작)
    void BeginAttackWindow();         // 스윙 창 오픈(지속 스윕 + 즉시 1회 판정)
    void EndAttackWindow(bool bForced = true); // 스윙 창 종료
    void FinishAttack();              // 리커버리 종료 → 공격 종료 처리
    void CancelAttack();              // 상태 전환/사망 등에서 모든 타이머 정리

    // ───────── 물리/설정 ─────────
    void SetupCapsulePhysics();

    // ───────── 헬퍼 ─────────
    AAIController* GetEnemyAIController() const;
    ACTacticalEnemyAIController* GetTacticalController() const;
    void StopMovement() const;
    void StopMovementAndFaceTarget();
    void RequestTacticalChase();
    bool ShouldEnterAttackFromChase() const;
    void HandleCooldownStrafe();
    void PlayMontageIfValid(UAnimMontage* Montage, float PlayRate = 1.f) const;
    void TryPlayIdleMontage() const;
    void PlaySoundIfValid(USoundBase* Sound) const;
    void SpawnAttackEffect() const;
    void SpawnHitEffectAtForward() const;
    void SpawnHitEffectAtLocation() const;
    void ClearAttackTimers();
    
protected:
    // ───────── 공격 설정(튜닝) ─────────
    /** 공격 주기(쿨다운) */
    UPROPERTY(EditAnywhere, Category="PF|Attack", meta=(ClampMin="0.1", ClampMax="5.0"))
    float AttackIntervalRiot = 1.0f;

    /** 공격 유효 거리(안전망) */
    UPROPERTY(EditAnywhere, Category="PF|Attack")
    float AttackRangeRiot = 200.f;

    /** 윈드업(예비동작) 시간 */
    UPROPERTY(EditAnywhere, Category="PF|Attack")
    float AttackWindUpTime = 0.25f;

    /** 스윙 창 지속 시간(분할 스윕 반복) */
    UPROPERTY(EditAnywhere, Category="PF|Attack")
    float AttackActiveWindow = 0.22f;

    /** 리커버리(후딜) 시간 */
    UPROPERTY(EditAnywhere, Category="PF|Attack")
    float AttackRecoveryTime = 0.35f;

    /** 일격 데미지 (BaseDamage와 동기화) */
    UPROPERTY(EditAnywhere, Category="PF|Attack")
    float AttackDamage = 15.f;

    // ───────── 물리 설정(군집 시 마찰 등) ─────────
    UPROPERTY(EditAnywhere, Category="PF|Physics")
    float CapsuleLinearDamping = 0.5f;

    UPROPERTY(EditAnywhere, Category="PF|Physics")
    float CapsuleAngularDamping = 0.5f;

    // ───────── 연출 ─────────
    UPROPERTY(EditAnywhere, Category="PF|Animation")
    UAnimMontage* AttackMontage = nullptr;

    UPROPERTY(EditAnywhere, Category="PF|Animation")
    UAnimMontage* IdleMontage = nullptr;

    UPROPERTY(EditAnywhere, Category="PF|Animation")
    UAnimMontage* DeadMontage = nullptr;
    
    UPROPERTY(EditAnywhere, Category="PF|Sound")
    USoundBase* AttackSound = nullptr;

    UPROPERTY(EditAnywhere, Category="PF|Sound")
    USoundBase* HitSound = nullptr;

    UPROPERTY(EditAnywhere, Category="PF|Effects")
    UNiagaraSystem* AttackEffect = nullptr;

    UPROPERTY(EditAnywhere, Category="PF|Effects")
    UNiagaraSystem* HitEffect = nullptr;

    // ───────── 상태 ─────────
    bool bIsAttacking = false;
    float AttackStartedTime = 0.f;

    // 타이머
    FTimerHandle Timer_WindUp;
    FTimerHandle Timer_EndWindow;
    FTimerHandle Timer_Finish;

    // 디버그
    UPROPERTY(EditAnywhere, Category="PF|Debug")
    bool bDebugAttackLog = false;
};