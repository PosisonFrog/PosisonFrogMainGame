#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CEnemyCharacterBase.generated.h"

class ACHealOrb;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEnemyDamaged, float, NewHealth, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEnemyDied);

/**
 * 공통 적 베이스
 * - HP/피격/사망 처리
 * - 피격 시 가벼운 노크백
 * - 사망 시 체력 구슬 스폰(옵션)
 */
UCLASS()
class POSISONFROG_API ACEnemyCharacterBase : public ACharacter
{
    GENERATED_BODY()

public:
    ACEnemyCharacterBase();

    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
        class AController* EventInstigator, AActor* DamageCauser) override;

    UFUNCTION(BlueprintCallable) FORCEINLINE bool  IsDead()       const { return bIsDead; }
    UFUNCTION(BlueprintCallable) FORCEINLINE float GetHealth()    const { return CurrentHealth; }
    UFUNCTION(BlueprintCallable) FORCEINLINE float GetMaxHealth() const { return MaxHealth; }

protected:
    virtual void BeginPlay() override;
    virtual void OnDeath();                // 사망 처리
    void         SpawnHealOrb();           // 체력 구슬 스폰
    void         ApplyKnockback(const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* Causer);

protected:
    // === Health ===
    UPROPERTY(EditAnywhere, Category = "Health", meta = (ClampMin = "1.0"))
    float MaxHealth = 100.f;

    UPROPERTY(VisibleInstanceOnly, Category = "Health")
    float CurrentHealth = 0.f;

    UPROPERTY(VisibleInstanceOnly, Category = "Health")
    bool  bIsDead = false;

    // === Knockback(피격 반응) ===
    /** 수평 노크백 속도(cm/s) */
    UPROPERTY(EditAnywhere, Category = "Hit|Knockback", meta = (ClampMin = "0.0"))
    float KnockbackSpeedXY = 600.f;

    /** 위로 살짝 튀기는 속도(cm/s) */
    UPROPERTY(EditAnywhere, Category = "Hit|Knockback", meta = (ClampMin = "0.0"))
    float KnockbackUpSpeed = 80.f;

    /** 연속 피격 시 노크백 쿨다운(초). 너무 자주 밀리지 않게 함 */
    UPROPERTY(EditAnywhere, Category = "Hit|Knockback", meta = (ClampMin = "0.0"))
    float KnockbackCooldown = 0.12f;

    /** 노크백 중 슬라이드 느낌을 주기 위한 임시 마찰값 */
    UPROPERTY(EditAnywhere, Category = "Hit|Knockback", meta = (ClampMin = "0.0"))
    float KnockbackGroundFriction = 1.0f;

    /** 임시 마찰 적용 시간(초). 짧게 미끄러지다 멈추는 느낌 */
    UPROPERTY(EditAnywhere, Category = "Hit|Knockback", meta = (ClampMin = "0.0"))
    float KnockbackFrictionTime = 0.18f;

    /** 마지막 노크백 시각 */
    UPROPERTY(VisibleInstanceOnly, Category = "Hit|Knockback")
    float LastKnockTime = -1000.f;

    // === Death Drop(체력 구슬) ===
    UPROPERTY(EditDefaultsOnly, Category = "Loot")
    TSubclassOf<ACHealOrb> HealOrbClass;

    /** 지면 보정 라인트레이스 오프셋(위/아래) */
    UPROPERTY(EditAnywhere, Category = "Loot")
    float TraceUpOffset = 30.f;

    UPROPERTY(EditAnywhere, Category = "Loot")
    float TraceDownOffset = 60.f;

    /** 사망 후 제거까지 대기 시간(즉시 삭제 원하면 0으로) */
    UPROPERTY(EditAnywhere, Category = "Death")
    float DeathLifeSpan = 0.15f;

public:
    // === 이벤트(HP UI/사운드/이펙트 연결용) ===
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnEnemyDamaged OnEnemyDamaged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnEnemyDied OnEnemyDied;
};

