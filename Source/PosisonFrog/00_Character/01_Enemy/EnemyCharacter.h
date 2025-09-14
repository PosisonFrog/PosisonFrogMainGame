#pragma once
// Source/PosisonFrogMainGame/Public/01_Enemy/EnemyCharacter.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "00_Character/00_Player/CPlayerCharacter.h"  
#include "EnemyCharacter.generated.h"

class ACPlayerCharacter;

UENUM(BlueprintType)
enum class EEnemyState : uint8
{
    Patrol,
    Alert,
    Chase,
    Attack,
    ReturnHome,
    Dead
};

UCLASS()
class POSISONFROG_API AEnemyCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AEnemyCharacter();

    virtual void Tick(float DeltaSeconds) override;
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
        class AController* EventInstigator, AActor* DamageCauser) override;

protected:
    virtual void BeginPlay() override;

private:
    // ===== FSM =====
    void UpdateFSM(float DeltaSeconds);
    void EnterState(EEnemyState NewState);
    bool ShouldUpdateAI(float DeltaSeconds);
    bool HasSightToPlayer() const;

    // ===== 이동 유틸 =====
    void MoveTowards(const FVector& Dest, float DesiredSpeed);

    // ===== 순찰 =====
    void EnsurePatrolGoal();
    void PickNewRoamGoal();
    bool Reached(const FVector& Goal, float Radius) const;

    // ===== 공격 =====
    void DoMeleeHit();
    bool CanAttack() const;

    // ===== 사망 =====
    void Die();

    // ===== 편의 =====
    FORCEINLINE bool IsPlayerValid() const { return IsValid(Player); }

private:
    // ─ 상태 ─
    UPROPERTY(VisibleAnywhere, Category = "Enemy|State")
    EEnemyState State = EEnemyState::Patrol;

    UPROPERTY() ACPlayerCharacter* Player = nullptr;

    bool bIsDead = false;

    // ─ 체력 ─
    UPROPERTY(EditAnywhere, Category = "Enemy|HP")
    float MaxHP = 100.f;

    UPROPERTY(VisibleAnywhere, Category = "Enemy|HP")
    float CurrentHP = 100.f;

    // ─ 어그로/인지 ─
    UPROPERTY(EditAnywhere, Category = "Enemy|Sense")
    float ChaseStartDistance = 800.f;   // 이 이내 + LoS → Alert
    UPROPERTY(EditAnywhere, Category = "Enemy|Sense")
    float ChaseStopDistance = 1200.f;  // 이 이상 or LoS 장시간 끊기면 추격 종료
    UPROPERTY(EditAnywhere, Category = "Enemy|Sense")
    float EasyChaseDistance = 200.f;   // 매우 가까우면 LoS 생략
    UPROPERTY(EditAnywhere, Category = "Enemy|Sense")
    float LoseSightGrace = 1.0f;    // LoS 끊겨도 이 시간 동안 추격 유지
    float LoseSightAcc = 0.f;

    // ─ Leash(원점 복귀) ─
    UPROPERTY(EditAnywhere, Category = "Enemy|Leash")
    float LeashMaxDistance = 1800.f; // 원점으로부터 이 이상 멀어지면 복귀
    UPROPERTY(EditAnywhere, Category = "Enemy|Leash")
    float ReturnHomeReachDist = 120.f;  // 원점 도달 판정 반경
    FVector HomeLocation;

    // ─ Alert(경계) ─
    UPROPERTY(EditAnywhere, Category = "Enemy|Alert")
    float AlertDuration = 0.5f;
    float AlertAcc = 0.f;

    // ─ 순찰(에디터 없이 랜덤 로밍 기본) ─
    UPROPERTY(EditAnywhere, Category = "Enemy|Patrol")
    bool bRandomPatrolAroundHome = true;
    UPROPERTY(EditAnywhere, Category = "Enemy|Patrol")
    float PatrolRoamRadius = 600.f;
    UPROPERTY(EditAnywhere, Category = "Enemy|Patrol")
    float PatrolPointReachRadius = 120.f;
    UPROPERTY(EditAnywhere, Category = "Enemy|Patrol")
    float PatrolWaitTime = 1.0f;
    float PatrolWaitAcc = 0.f;

    // 고정 경로를 쓰고 싶다면 여기에 포인트(월드 좌표) 추가
    UPROPERTY(EditAnywhere, Category = "Enemy|Patrol")
    TArray<FVector> PatrolPoints;
    int32 PatrolIndex = 0;

    FVector CurrentPatrolGoal = FVector::ZeroVector;

    // ─ 공격 ─
    UPROPERTY(EditAnywhere, Category = "Enemy|Attack")
    float AttackDamage = 10.f;
    UPROPERTY(EditAnywhere, Category = "Enemy|Attack")
    float AttackCooldown = 1.2f;
    UPROPERTY(EditAnywhere, Category = "Enemy|Attack")
    float AttackEnterDistance = 140.f;

    UPROPERTY(EditAnywhere, Category = "Enemy|Attack")
    float AttackRadius = 45.f;
    UPROPERTY(EditAnywhere, Category = "Enemy|Attack")
    float AttackRange = 120.f;
    UPROPERTY(EditAnywhere, Category = "Enemy|Attack")
    float AttackOffset = 50.f;
    float LastAttackTime = -10000.f;

    // ─ 이동 스피드 ─
    UPROPERTY(EditAnywhere, Category = "Enemy|Move")
    float PatrolMoveSpeed = 260.f;
    UPROPERTY(EditAnywhere, Category = "Enemy|Move")
    float ChaseMoveSpeed = 420.f;

    // ─ 저비용 틱 ─
    UPROPERTY(EditAnywhere, Category = "Enemy|Perf")
    float NearThinkDistance = 900.f;   // 이 이내는 매프레임 갱신
    UPROPERTY(EditAnywhere, Category = "Enemy|Perf")
    float CheapThinkInterval = 0.15f;   // 멀면 이 간격으로만 갱신
    float CheapAcc = 0.f;
};
