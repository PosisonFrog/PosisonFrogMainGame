// Source/PosisonFrog/00_Character/01_Enemy/Components/CTankerChargeComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CTankerChargeComponent.generated.h"

class AAIController;
class ACharacter;
class AActor;
class UAnimMontage;
class UCharacterMovementComponent;
class UDamageType;


UENUM(BlueprintType)
enum class EChargeState : uint8
{
    Idle,
    PreCharge,   // 플레이어 정면이 아닌 좌/우 오프셋 지점으로 접근(줄서기 방지)
    Windup,      // 예고(전조) 구간 - 애님/사운드
    Charging,    // 실제 돌진
    Recovery,    // 히트/충돌 이후 후딜(자체 스턴 포함)
    Cooldown     // 쿨타임
};

UENUM(BlueprintType)
enum class EChargeEndReason : uint8
{
    None,
    HitPawn,
    HitWorld,
    MaxTime,
    MaxDistance,
    Aborted
};

// 상태 변경/종료 브로드캐스트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnChargeStateChanged, EChargeState, NewState, EChargeState, PreviousState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnChargeFinished, EChargeEndReason, Reason, AActor*, HitActor);


/*
 * Behaviour component that drives the tanker's multi-step charge move.
 * This is a full rewrite that preserves the original feature set and tunable data.
*/

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class POSISONFROG_API UCTankerChargeComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCTankerChargeComponent();

    // 외부 요청: 타겟을 향해 돌진 시작(조건 충족 시 true)
    UFUNCTION(BlueprintCallable, Category = "PF|Charge")
    bool RequestCharge(AActor* InTarget);

    // 강제 중단(디버그/리셋용)
    UFUNCTION(BlueprintCallable, Category = "PF|Charge")
    void AbortCharge();
    
    UFUNCTION(BlueprintPure, Category = "PF|Charge")
    bool IsChargingOrWindup() const;
    
    UFUNCTION(BlueprintPure, Category = "PF|Charge")
    bool IsOnCooldown() const { return State == EChargeState::Cooldown; }

    UFUNCTION(BlueprintPure, Category = "PF|Charge")
    EChargeState GetState() const { return State; }

    // 애님 노티: Windup → Charging 개시
    UFUNCTION(BlueprintCallable, Category = "PF|Charge|Anim")
    void Anim_ChargeStart();

public:
    UPROPERTY(BlueprintAssignable)
    FOnChargeStateChanged OnChargeStateChanged;
    
    UPROPERTY(BlueprintAssignable)
    FOnChargeFinished    OnChargeFinished;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    void EnterState(EChargeState NewState);
    
    // 단계 전환
    void ResetTransientData();
    void ClearTimers();

    void BeginWindupInternal();
    void BeginChargingInternal();
    void EndChargingInternal(EChargeEndReason Reason, AActor* HitActor);
    void BeginRecoveryInternal(EChargeEndReason Reason, AActor* HitActor);
    void StartCooldownInternal();
    void HandleMaxChargeTime();
    void HandleCooldownFinished();

    // Pre-charge steering helpers
    bool TryBuildPreChargeGoal(FVector& OutGoal, int32& OutSideSign);
    void TickPreCharge(float DeltaSeconds);
    bool HasReached(const FVector& Point, float Radius) const;
    
    // Charging trace helpers
    void UpdateCharging(float DeltaSeconds);
    void PerformChargeTrace();
    bool SweepAhead(FHitResult& OutHit, float Distance) const;
    
    // Validation helpers
    bool EnsureOwnerAndMovement();
    bool HasValidTarget() const;
    float DistanceToTarget2D() const;
    void FaceTowards(const FVector& Direction, float DeltaSeconds);

private:
    // ─ Gate(시작 거리 조건) ─
    UPROPERTY(EditDefaultsOnly, Category = "PF|Charge|Gate")
    float ChargeMinDistance = 600.f;

    UPROPERTY(EditDefaultsOnly, Category = "PF|Charge|Gate")
    float ChargeMaxDistance = 1600.f;

    // ─ PreCharge(사선 오프셋 접근) ─
    UPROPERTY(EditDefaultsOnly, Category = "PF|Charge|PreCharge")
    bool  bUsePreChargeOffset = true;

    UPROPERTY(EditDefaultsOnly, Category = "PF|Charge|PreCharge", meta = (ClampMin = "100"))
    float PreChargeDistance = 500.f;                // 타겟 기준 반경

    UPROPERTY(EditDefaultsOnly, Category = "PF|Charge|PreCharge", meta = (ClampMin = "0"))
    float PreChargeLateralOffset = 260.f;           // 좌/우 편향

    UPROPERTY(EditDefaultsOnly, Category = "PF|Charge|PreCharge", meta = (ClampMin = "60"))
    float PreChargeAcceptanceRadius = 140.f;

    UPROPERTY(EditDefaultsOnly, Category = "PF|Charge|PreCharge", meta = (ClampMin = "0.3", ClampMax = "5.0"))
    float PreChargeMaxTime = 1.6f;                  // 실패 폴백(시간 초과)

    UPROPERTY(EditDefaultsOnly, Category = "PF|Charge|PreCharge")
    bool  bAlternateSideBetweenCharges = true;      // 좌/우 번갈이

    UPROPERTY(EditDefaultsOnly, Category = "PF|Charge|PreCharge")
    bool  bClampToNavMesh = true;                   // NavMesh 보정

    UPROPERTY(EditDefaultsOnly, Category = "PF|Charge|PreCharge", meta = (ClampMin = "200"))
    float PreChargeMoveSpeed = 600.f;               // 비Nav 시 직진 속도

    // ─ Windup/Charging ─
    UPROPERTY(EditDefaultsOnly, Category = "PF|Charge|Windup")
    float WindupTime = 0.6f;

    UPROPERTY(EditDefaultsOnly, Category = "PF|Charge|Anim")
    bool bStartOnAnimNotify = true;                // 노티 없이도 폴백 타이머로 시작

    UPROPERTY(EditDefaultsOnly, Category = "PF|Charge|Move", meta = (ClampMin = "400"))
    float ChargeSpeed = 1500.f;

    UPROPERTY(EditDefaultsOnly, Category = "PF|Charge|Move", meta = (ClampMin = "90"))
    float TurnRateDegPerSec = 360.f;

    UPROPERTY(EditDefaultsOnly, Category = "PF|Charge|Move", meta=(ClampMin = "0.5", ClampMax = "5.0"))
    float MaxChargeTime = 2.0f;

    // ─ Damage/Hit ─
    UPROPERTY(EditDefaultsOnly, Category = "PF|Charge|Damage", meta = (ClampMin = "0"))
    float HitDamage = 45.f;

    UPROPERTY(EditDefaultsOnly, Category = "PF|Charge|Damage")
    TSubclassOf<UDamageType> DamageTypeClass;

    UPROPERTY(EditDefaultsOnly, Category = "PF|Charge|Hit", meta = (ClampMin = "0"))
    float WallStunTime = 1.0f;

    UPROPERTY(EditDefaultsOnly, Category = "PF|Charge|Hit", meta=(ClampMin = "0"))
    float RecoveryTime = 0.6f;
    
    UPROPERTY(EditDefaultsOnly, Category = "PF|Charge|Hit", meta=(ClampMin = "0"))
    float FailedChargeRecoveryDelay = 1.0f;
    
    // ─ Cooldown ─
    UPROPERTY(EditDefaultsOnly, Category = "PF|Charge|Cooldown", meta = (ClampMin = "0"))
    float ChargeCooldown = 5.0f;

    // ─ Trace ─
    UPROPERTY(EditDefaultsOnly, Category = "PF|Charge|Trace", meta = (ClampMin = "20"))
    float TraceRadius = 60.f;

    UPROPERTY(EditDefaultsOnly, Category = "PF|Charge|Trace", meta = (ClampMin = "60"))
    float TraceAhead  = 120.f;

    UPROPERTY(EditDefaultsOnly, Category = "PF|Charge|Debug")
    bool  bDrawPreChargeGoal = false;



private:
    // ─ Runtime ─
    UPROPERTY(Transient)
    EChargeState State = EChargeState::Idle;

    TWeakObjectPtr<ACharacter> OwnerChar;
    TWeakObjectPtr<UCharacterMovementComponent> MoveComp;
    TWeakObjectPtr<AActor> TargetActor;
    TWeakObjectPtr<AAIController> CachedAI;
 
    

    // Charging
    FVector ChargeDirection = FVector::ForwardVector;
    float ChargeStartTime = 0.f;
    TSet<TWeakObjectPtr<AActor>> HitActorsThisCharge;

    // PreCharge
    FVector PreChargeGoal = FVector::ZeroVector;
    bool bPreChargeUsingNav = false;
    float PreChargeStartTime = 0.f;
    int32 LastSideSign = +1;
    
    // Timers
    FTimerHandle TH_Windup;
    FTimerHandle TH_MaxCharge;
    FTimerHandle TH_Recovery;
    FTimerHandle TH_Cooldown;
    FTimerHandle TH_PreChargeTimeout;

    bool bPendingFailedChargeRecovery = false;
};
