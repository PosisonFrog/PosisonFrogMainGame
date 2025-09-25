// CEnemyCharacterBase.h
#pragma once

#include "CoreMinimal.h"
#include "00_Character/CBaseCharacter.h"
#include "CEnemyCharacterBase.generated.h"

class UCapsuleComponent;
class UCharacterMovementComponent;
class UCHealthComponent;

/** 적 상태(FSM) */
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

/**
 * 공통 적 베이스 (보스 제외)
 * - LoS(거리 + FOV + 라인트레이스) 기반 인지 및 전이
 * - bUseNavigation: NavMesh 있으면 MoveTo / 없으면 AddMovementInput
 * - 순찰/추적/공격/귀환/사망 공통 로직
 */
UCLASS()
class POSISONFROG_API ACEnemyCharacterBase : public ACBaseCharacter
{
    GENERATED_BODY()
public:
    ACEnemyCharacterBase();

    /** 현재 상태 조회 */
    UFUNCTION(BlueprintPure) EEnemyState GetState() const { return State; }

    // ───────── 설정(시야/인지) ─────────
    UPROPERTY(EditAnywhere, Category="PF|AI|Sense", meta=(ClampMin="0"))
    float SightDistance = 1200.f;          // 시야 거리

    UPROPERTY(EditAnywhere, Category="PF|AI|Sense", meta=(ClampMin="0", ClampMax="180"))
    float SightFOVDegrees = 80.f;          // 시야 콘(전방 ±FOV/2)

    UPROPERTY(EditAnywhere, Category="PF|AI|Sense")
    TEnumAsByte<ECollisionChannel> SightTraceChannel = ECC_Visibility;  // 시야 트레이스 채널

    UPROPERTY(EditAnywhere, Category="PF|AI|Sense")
    float SightHeightOffsetSelf = 60.f;     // 내 시점 높이 보정
    UPROPERTY(EditAnywhere, Category="PF|AI|Sense")
    float SightHeightOffsetTarget = 50.f;   // 타겟 시점 높이 보정

    UPROPERTY(EditAnywhere, Category="PF|AI|Sense")
    float ChaseStartDistance = 1200.f;      // 추적 시작(거리 기준)
    UPROPERTY(EditAnywhere, Category="PF|AI|Sense")
    float ChaseStopDistance  = 2000.f;      // 추적 포기(멀어짐)
    UPROPERTY(EditAnywhere, Category="PF|AI|Sense")
    float LoseSightGrace     = 1.0f;        // 시야 상실 유예 시간

    // ───────── 설정(공격) ─────────
    UPROPERTY(EditAnywhere, Category="PF|AI|Attack")
    float AttackEnterDistance = 160.f;      // 이내면 Attack 진입
    UPROPERTY(EditAnywhere, Category="PF|AI|Attack")
    float AttackExitDistance  = 220.f;      // 벗어나면 Chase 복귀

    UPROPERTY(EditAnywhere, Category="PF|AI|Attack")
    float AttackRange  = 180.f;             // 근접 판정 거리
    UPROPERTY(EditAnywhere, Category="PF|AI|Attack")
    float AttackRadius = 60.f;              // (필요 시 사용)

    UPROPERTY(EditAnywhere, Category="PF|AI|Attack")
    float AttackInterval = 1.0f;            // 기본 공격 주기
    UPROPERTY(EditAnywhere, Category="PF|Combat")
    float BaseDamage = 10.f;                // 기본 데미지

    // ───────── 설정(순찰/이동) ─────────
    UPROPERTY(EditAnywhere, Category="PF|AI|Patrol")
    float PatrolRoamRadius = 800.f;
    UPROPERTY(EditAnywhere, Category="PF|AI|Patrol")
    float PatrolWaitTime = 1.5f;
    UPROPERTY(EditAnywhere, Category="PF|AI|Patrol")
    float PatrolPointReachRadius = 120.f;

    /** NavMesh 유무와 무관하게 동작하도록 모드 토글 */
    UPROPERTY(EditAnywhere, Category="PF|AI|Nav")
    bool bUseNavigation = true;                 // true: MoveTo, false: 직진 스티어링
    UPROPERTY(EditAnywhere, Category="PF|AI|Nav", meta=(EditCondition="!bUseNavigation", ClampMin="0"))
    float DirectMoveSpeed = 360.f;              // 직진 모드 이동 속도

    // ───────── 성능(연산 빈도) ─────────
    UPROPERTY(EditAnywhere, Category="PF|AI|Perf")
    float NearThinkDistance   = 2500.f;         // 근거리 고빈도 기준
    UPROPERTY(EditAnywhere, Category="PF|AI|Perf")
    float CheapThinkInterval  = 0.25f;          // 원거리 저빈도
    UPROPERTY(EditAnywhere, Category="PF|AI|Perf")
    float RichThinkInterval   = 0.05f;          // 근거리 고빈도

    // ───────── 디버그 ─────────
    UPROPERTY(EditAnywhere, Category="PF|Debug")
    bool bShowDebugInfo = false;                // 디버그 정보 표시

protected:
    // AActor
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    // FSM 프레임
    virtual void Think(float DeltaTime);
    virtual void EnterState(EEnemyState NewState);
    virtual void ExitState(EEnemyState OldState);
    void        SetState(EEnemyState NewState);

    // 상태 처리
    virtual void DoPatrol();
    virtual void DoAlert();
    virtual void DoChase();
    virtual void DoAttack();     // 타입별로 오버라이드 가능
    virtual void DoReturnHome();
    virtual void DoDead();

    // 조건/헬퍼
    virtual bool IsAttackReady() const;
    virtual bool IsInAttackDistance() const;

    bool  AcquireTarget();
    bool  HasVisualOnTarget() const;            // 거리+FOV+라인트레이스
    bool  IsTargetInFOV(const AActor* Other) const;
    float DistToTarget() const;

    // 이동(Nav/직진 겸용)
    void  RequestMoveTo(const FVector& Goal, float AcceptanceRadius = 120.f);
    void  StopMove();
    bool  Reached(const FVector& P, float Radius) const;
    void  DirectMoveTick(float DeltaSeconds);

    // 데미지/사망/드랍
    virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
                             AController* EventInstigator, AActor* DamageCauser) override;
    UFUNCTION() void OnHealthChanged(float Cur, float Max);
    virtual void OnDead();
    virtual void TryDropHealPack();
    
    // 애니메이션 연동
    UFUNCTION(BlueprintCallable, Category="PF|Combat")
    void ApplyAttackDamage();                   // 애니메이션 노티파이에서 호출
    
    // 디버그
    void DebugDrawState();

protected:
    // 런타임 상태/캐시
    UPROPERTY(Transient) TObjectPtr<AActor> Target = nullptr;
    UPROPERTY(VisibleInstanceOnly, Category="PF|AI") EEnemyState State = EEnemyState::Patrol;

    FVector HomeLocation = FVector::ZeroVector;
    FVector PatrolGoal   = FVector::ZeroVector;

    float   NextThinkTime = 0.f;
    float   LastSeenTime  = -1000.f;
    float   LastAttackTime = -1000.f;
    float   StateEnterTime = -1000.f;          // 상태 진입 시간

    // 직진 모드 이동 상태
    bool    bDirectMoveActive = false;
    FVector DirectMoveGoal = FVector::ZeroVector;
    float   DirectAcceptanceRadius = 120.f;
    
    // 드롭 설정
    UPROPERTY(EditAnywhere, Category="PF|Drop")
    float HealPackDropChance = 0.3f;           // 체력 팩 드롭 확률
    UPROPERTY(EditAnywhere, Category="PF|Drop")
    TSubclassOf<AActor> HealPackClass;          // 드롭할 체력 팩 클래스
};

