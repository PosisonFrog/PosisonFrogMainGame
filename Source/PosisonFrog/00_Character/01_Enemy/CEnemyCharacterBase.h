#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"                // ECollisionChannel
#include "00_Character/CBaseCharacter.h"
#include "CEnemyCharacterBase.generated.h"

class UNiagaraSystem;
class UCapsuleComponent;
class UCharacterMovementComponent;
class UCEnemyHealthComponent;
class UCEnemyWeaponComponent;
class USoundBase;
class UAnimMontage;
class UCPawnLifecycleSubsystem;
class ACPlayerCharacter;

UENUM(BlueprintType)
enum class EEnemyHitDirection : uint8
{
    None,
    FromLeft,
    FromRight
};

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
 * - LoS(거리 + FOV + 라인트레이스) 인지
 * - NavMesh 유무에 따라 MoveTo / 직진 스티어링 겸용(길찾기 필요 없을때 그냥 직진)
 * - 근접 공격: 스윙창 + 분할 스윕(프레임 독립 - 터널링(통과 현상 막음.) + Overlap 보조(Sweep 놓칠 시 대비) + 거리 안전망
 * - 군집 추적 개선: 포위(링 오프셋 : 한점으로 안모이고 원 형태로 모게) + Separation(분리) + RVO(충돌 회피 알고리즘)
 */
UCLASS(config=Game)
class POSISONFROG_API ACEnemyCharacterBase : public ACBaseCharacter
{
    GENERATED_BODY()

public:
    ACEnemyCharacterBase();

    UFUNCTION(BlueprintPure, Category="PF|AI")
    EEnemyState GetState() const { return State; }

    // ───────── 시야/인지 ─────────
    UPROPERTY(EditAnywhere, Category="PF|AI|Sense", meta=(ClampMin="0"))
    float SightDistance = 1200.f;

    UPROPERTY(EditAnywhere, Category="PF|AI|Sense", meta=(ClampMin="0", ClampMax="180"))
    float SightFOVDegrees = 80.f;

    UPROPERTY(EditAnywhere, Category="PF|AI|Sense")
    TEnumAsByte<ECollisionChannel> SightTraceChannel = ECC_Visibility;

    UPROPERTY(EditAnywhere, Category="PF|AI|Sense")
    float SightHeightOffsetSelf = 60.f;

    UPROPERTY(EditAnywhere, Category="PF|AI|Sense")
    float SightHeightOffsetTarget = 50.f;

    UPROPERTY(EditAnywhere, Category="PF|AI|Sense")
    float ChaseStartDistance = 1200.f;

    UPROPERTY(EditAnywhere, Category="PF|AI|Sense")
    float ChaseStopDistance  = 2000.f;

    UPROPERTY(EditAnywhere, Category="PF|AI|Sense")
    float LoseSightGrace     = 1.0f;

    // ───────── 공격 주기/거리 ─────────
    UPROPERTY(EditAnywhere, Category="PF|AI|Attack", meta=(ClampMin="0"))
    float AttackInterval = 1.0f;

    UPROPERTY(EditAnywhere, Category="PF|AI|Attack")
    float AttackEnterDistance = 160.f;

    UPROPERTY(EditAnywhere, Category="PF|AI|Attack")
    float AttackExitDistance  = 220.f;

    // Attack 상태에서의 재접근/복귀 파라미터
    UPROPERTY(EditDefaultsOnly, Category="PF|AI|Attack", meta=(ClampMin="0"))
    float AttackReengageDelay = 0.1f; // 공격 후 재추격 시작까지 짧은 지연

    UPROPERTY(EditDefaultsOnly, Category="PF|AI|Attack", meta=(ClampMin="0"))
    float AttackMoveAcceptanceRadius = 120.f; // Attack 상태에서 타겟 접근 허용 반경

    // 내부 상태 플래그
    UPROPERTY(VisibleInstanceOnly, Category="PF|AI")
    bool bIsPerformingMelee = false;

    UPROPERTY(EditAnywhere, Category="PF|Combat")
    float BaseDamage = 10.f;

    // ───────── 스윕(근접 판정) ─────────
    UPROPERTY(EditAnywhere, Category="PF|AI|Attack|Sweep", meta=(ClampMin="0"))
    float AttackRange = 180.f;                 // 보조 거리 안전망

    UPROPERTY(EditAnywhere, Category="PF|AI|Attack|Sweep", meta=(ClampMin="0"))
    float AttackSweepLength = 120.f;

    UPROPERTY(EditAnywhere, Category="PF|AI|Attack|Sweep", meta=(ClampMin="0"))
    float AttackSweepRadius = 45.f;

    UPROPERTY(EditAnywhere, Category="PF|AI|Attack|Sweep", meta=(ClampMin="0"))
    float AttackSweepHalfHeight = 20.f;

    UPROPERTY(EditAnywhere, Category="PF|AI|Attack|Sweep")
    FVector AttackOriginOffset = FVector(0,0,50);

    UPROPERTY(EditAnywhere, Category="PF|AI|Attack|Sweep", meta=(ClampMin="0", ClampMax="180"))
    float AttackArcDegrees = 120.f;

    UPROPERTY(EditAnywhere, Category="PF|AI|Attack|Sweep")
    TEnumAsByte<ECollisionChannel> AttackTraceChannel = ECC_Pawn;

    UPROPERTY(EditAnywhere, Category="PF|AI|Attack|Sweep")
    bool bAttackHitOnlyPlayers = true;

    UPROPERTY(EditAnywhere, Category="PF|AI|Attack|Sweep", meta=(ClampMin="0.01", ClampMax="0.2"))
    float AttackSweepInterval = 0.05f;

    UPROPERTY(EditAnywhere, Category="PF|AI|Attack|Sweep", meta=(ClampMin="1", ClampMax="10"))
    int32 MaxSweepsPerFrame = 3;

    // ───────── 순찰/이동 ─────────
    UPROPERTY(EditAnywhere, Category="PF|AI|Patrol")
    float PatrolRoamRadius = 800.f;

    UPROPERTY(EditAnywhere, Category="PF|AI|Patrol")
    float PatrolWaitTime = 1.5f;

    UPROPERTY(EditAnywhere, Category="PF|AI|Patrol")
    float PatrolPointReachRadius = 120.f;

    UPROPERTY(EditAnywhere, Category="PF|AI|Nav")
    bool bUseNavigation = true;                // false면 직진 스티어링

    UPROPERTY(EditAnywhere, Category="PF|AI|Nav", meta=(EditCondition="!bUseNavigation", ClampMin="0"))
    float DirectMoveSpeed = 360.f;

    // ───────── 군집 추적 개선 ─────────
    // 링(포위) 오프셋
    UPROPERTY(EditAnywhere, Category="PF|AI|Chase")
    bool bUseChaseRing = true;

    UPROPERTY(EditAnywhere, Category="PF|AI|Chase", meta=(ClampMin="0"))
    float ChaseRingPadding = 80.f;             // AttackEnterDistance에 추가

    UPROPERTY(EditAnywhere, Category="PF|AI|Chase", meta=(ClampMin="0", ClampMax="360"))
    float ChaseRingAngleJitterDeg = 25.f;

    // Separation
    UPROPERTY(EditAnywhere, Category="PF|AI|Separation")
    bool bUseSeparation = true;

    UPROPERTY(EditAnywhere, Category="PF|AI|Separation", meta=(ClampMin="0"))
    float SeparationRadius = 140.f;

    UPROPERTY(EditAnywhere, Category="PF|AI|Separation", meta=(ClampMin="0"))
    float SeparationStrength = 300.f;

    // RVO
    UPROPERTY(EditAnywhere, Category="PF|AI|Avoidance")
    bool bUseRVOAvoidance = true;

    UPROPERTY(EditAnywhere, Category="PF|AI|Avoidance", meta=(ClampMin="0"))
    float RVOAvoidanceRadius = 48.f;

    UPROPERTY(EditAnywhere, Category="PF|AI|Avoidance", meta=(ClampMin="0"))
    float RVOConsiderationRadius = 600.f;

    UPROPERTY(EditAnywhere, Category="PF|AI|Avoidance", meta=(ClampMin="0", ClampMax="1"))
    float RVOAvoidanceWeight = 0.6f;
    
    // ───────── 성능/디버그/드롭 ─────────

    // 가까운 생각 거리
    UPROPERTY(EditAnywhere, Category="PF|AI|Perf")
    float NearThinkDistance   = 2500.f;

    // 저비용 생각 주기 : 0.25초
    UPROPERTY(EditAnywhere, Category="PF|AI|Perf")
    float CheapThinkInterval  = 0.25f;

    // 고비용 생각 주기 : 0.05초
    UPROPERTY(EditAnywhere, Category="PF|AI|Perf")
    float RichThinkInterval   = 0.05f;

    UPROPERTY(EditAnywhere, Category="PF|Drop", meta=(ClampMin="0", ClampMax="1"))
    float HealPackDropChance = 0.3f;

    UPROPERTY(EditAnywhere, Category="PF|Drop")
    TSubclassOf<AActor> HealPackClass;

    UPROPERTY(EditAnywhere, Category="PF|Debug")
    bool bShowDebugInfo = false;

    UPROPERTY(EditAnywhere, Category="PF|Debug")
    bool bDebugDrawAttack = false;

protected:
    // AActor
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override; 
    virtual void Tick(float DeltaSeconds) override;

    // FSM
    virtual void Think(float DeltaTime);                // 현재 상태에 따라 적절한 행동
    virtual void EnterState(EEnemyState NewState);      // 새 상태 진입시 호출, 상태 초기화 작업.
    virtual void ExitState(EEnemyState OldState);       // 현 상태에서 벗어날 시 호출. 정리 작업.
    void         SetState(EEnemyState NewState);        // 상태 변경 함수. 위 두 함수 순서대로 호출.
    
    // 상태 처리
    virtual void DoPatrol();     //순찰
    virtual void DoAlert();      //경계
    virtual void DoChase();      //추적
    virtual void DoAttack();     //공격
    virtual void DoReturnHome(); //복귀
    virtual void DoDead();       //죽음

    // 조건/헬퍼 - 말 그대로임. 
    virtual bool IsAttackReady() const;       
    virtual bool IsInAttackDistance() const;

    bool  AcquireTarget();                             // 타겟(플레이어) 찾고 설정
    virtual bool  HasVisualOnTarget() const;           // 타겟이 지금 보이는지
    bool  IsTargetInFOV(const AActor* Other) const;    // 타겟이 장애물에 안가려지고 시야범위에 있는지
    float DistToTarget() const;                        // 타겟까지의 거리계산

    // 이동(Nav/직진)
    void  RequestMoveTo(const FVector& Goal, float AcceptanceRadius = 120.f);  // 목표지점 이동 bUseNavigation에 따라 길찾기나 직진 스티어링
    void  StopMove();                                                          // 멈춤(말 그대로임).     
    bool  Reached(const FVector& P, float Radius) const;                       // 현재 위치가 목표지점 P의 반경 Radius에 도달했는지 확인
    void  DirectMoveTick(float DeltaSeconds);                                  // 직진 스티어링 일때, 매틱마다 타겟쪽으로 움직임

    // 데미지/사망/드롭 - 말 그대로임.
    virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
                             AController* EventInstigator, AActor* DamageCauser) override;
    UFUNCTION() void OnHealthChanged(float Cur, float Max);
    virtual void OnDead();

    // 플레이어 콤보 공격 피격 처리
    UFUNCTION()
    void OnPlayerComboHit(AActor* HitActor, int32 ComboIndex, float Damage);
    void SubscribeToPlayerComboHits();
    void UnsubscribeFromPlayerComboHits();
    virtual void TryDropHealPack();
    void EndHitStun();  // 피격 경직 종료

    // ───────── 공통 유틸리티 함수 ─────────
    /** 몽타주가 유효하면 재생 */
    void PlayMontageIfValid(UAnimMontage* Montage, float PlayRate = 1.f) const;
    
    // 전투(스윙 창(애님 노티파이) + 분할 스윕)
    UFUNCTION(BlueprintCallable, Category="PF|Combat")
    void AttackWindowBegin(float AutoEndAfter = 0.f);

    UFUNCTION(BlueprintCallable, Category="PF|Combat")
    void AttackWindowEnd(bool bForce = true);

    UFUNCTION(BlueprintCallable, Category="PF|Combat")
    bool ApplyAttackDamage(bool bCheckAngle = true);

    // 보조 함수 선언
    void EnsureWalkingAndResume();
    void ReengageChase(float DelaySec = 0.f);

    // Ai 현 상태 디버깅
    void DebugDrawState(); // Ai 현 상태 디버깅

private:
    void PerformAttackSweep();                              // 분할 스윕 핵심 함수 -> 짧은 간격으로 스윕 트레이스 수행.
    bool PassAngleFilter(const AActor* Other) const;        // 공격에 맞은 액터가 실제 공격 각도 안에 있는가?
    bool IsValidAttackTarget(AActor* Other) const;          // 공격 판정으로 피해를 줄 수 있는 대상인가?

public:
    // 스폰 시 초기 위치 저장
    void SaveInitialTransform();

    // 리스폰 시 초기 위치로 복귀
    void ResetToInitialTransform();
    void ResetForRespawn();

    UFUNCTION(BlueprintNativeEvent, Category = "Enemy|Respawn")
    void OnResetForRespawn();
    virtual void OnResetForRespawn_Implementation();
    
    UFUNCTION(BlueprintNativeEvent, Category = "Enemy|Respawn")
    void OnRespawned();
    virtual void OnRespawned_Implementation();
    
    void ForceRestartAI();

    // 커맨드 공격으로 띄워질 때 랜덤 애니메이션 출력
    UFUNCTION(BlueprintCallable, Category="PF|LaunchReaction")
    void PlayRandomLaunchReaction();

    // 커맨드 공격으로 내려찍힐 때 애니메이션 재생
    UFUNCTION(BlueprintCallable, Category="PF|LaunchReaction")
    void PlaySlamImpactReaction();
    
protected:
    virtual void HandlePlayerRespawned(class ACPlayerCharacter* NewPlayer);
    
protected:
    // 런타임 상태
    UPROPERTY(Transient) TObjectPtr<AActor> Target = nullptr;

    UPROPERTY(VisibleInstanceOnly, Category="PF|AI")
    EEnemyState State = EEnemyState::Patrol;

    FVector HomeLocation = FVector::ZeroVector;
    FVector PatrolGoal   = FVector::ZeroVector;

    float   NextThinkTime  = 0.f;
    float   LastSeenTime   = -1000.f;
    float   LastAttackTime = -1000.f;
    float   StateEnterTime = -1000.f;
    
    // 직진 스티어링
    bool    bDirectMoveActive = false;
    FVector DirectMoveGoal = FVector::ZeroVector;
    float   DirectAcceptanceRadius = 120.f;

    // 스윙 창
    bool    bAttackWindowActive = false;
    float   AttackWindowEndTime = -1.f;
    TSet<TWeakObjectPtr<AActor>> SwingHitActors;

    // 스윕 타이밍
    float   LastAttackSweepTime = 0.f;

    // 포위 각도 시드
    float   MyChaseAngleDeg = 0.f;

    // 리스폰을 위한 초기 위치 저장
    FVector InitialSpawnLocation = FVector::ZeroVector;
    FRotator InitialSpawnRotation = FRotator::ZeroRotator;
    FTimerHandle HitStunTimer;  // 피격 경직 타이머
    FTimerHandle HitShakeTimer; // 셰이킹 타이머
    FVector OriginalMeshLocation = FVector::ZeroVector;
    bool bIsShaking = false;
    float HitShakeElapsed = 0.f;


    // 피격 경직
    UPROPERTY(EditAnywhere, Category="PF|HitReaction", meta=(ClampMin="0", ClampMax="5"))
    float HitStunDuration = 0.4f;  // 피격 경직 지속 시간 (초)

    UPROPERTY(VisibleInstanceOnly, Category="PF|HitReaction")
    bool bIsHitStunned = false;  // 피격 경직 상태

    UPROPERTY(EditAnywhere, Category="PF|HitReaction", meta=(ClampMin="0", ClampMax="2"))
    float HitShakeDuration = 0.5f;  // 쉐이킹 지속 시간

    UPROPERTY(EditAnywhere, Category="PF|HitReaction", meta=(ClampMin="0", ClampMax="100"))
    float HitShakeIntensity = 15.f;  // 쉐이킹 세기 (위아래 움직임)
    
    UPROPERTY(EditAnywhere, Category="PF|HitReaction", meta=(ClampMin="0", ClampMax="200"))
    float HitShakeFrequency = 20.f;  // 쉐이킹 속도 (높을수록 빠름)

    // 피격 VFX
    UPROPERTY(EditAnywhere, Category = "Effects|Damage")
    UNiagaraSystem* TakeHitEffect = nullptr;

    void SpawnDamageReceivedEffect(const FVector& HitLocation, const FVector& HitNormal);
    
    void StartHitShake();
    void UpdateHitShake();
    void StopHitShake();
    
    // 애니메이션
    UPROPERTY(EditAnywhere, Category="PF|Animation")
    TArray<UAnimMontage*> HitReactionMontagesLeft;
    
    UPROPERTY(EditAnywhere, Category="PF|Animation")
    TArray<UAnimMontage*> HitReactionMontagesRight;
    
    UPROPERTY(EditAnywhere, Category="PF|Animation")
    TArray<UAnimMontage*> HitReactionMontages;

    /** 플레이어 콤보별 피격 반응 몽타주 (인덱스: 0=1타, 1=2타, 2=3타). 비어있으면 HitReactionMontages 사용 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PF|Animation|ComboReaction")
    TArray<UAnimMontage*> ComboHitReactionMontages;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PF|Animation|ComboReaction")
    TArray<UAnimMontage*> ComboHitReactionMontagesLeft;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PF|Animation|ComboReaction")
    TArray<UAnimMontage*> ComboHitReactionMontagesRight;

    // 커맨드 공격에 의해 띄워질 때 재생할 몽타주
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PF|Animation|LaunchReaction")
    TArray<UAnimMontage*> LaunchReactionMontages;

    // 커맨드 공격으로 내려찍힐 때 재생할 몽타주
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PF|Animation|LaunchReaction")
    UAnimMontage* SlamImpactReactionMontage = nullptr;
    
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "PF|Animation")
    int32 PlayerCurrentCombo = 0;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "PF|HitReaction")
    EEnemyHitDirection LastHitDirection = EEnemyHitDirection::None;
    
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "PF|HitReaction")
    float LastHitDirectionRightDot = 0.f;
    
    //체력 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PF|Components")
    UCEnemyHealthComponent* HealthComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PF|Components")
    UCEnemyWeaponComponent* WeaponComponent;

    void UpdateHitDirectionFromAttacker(AActor* AttackerActor);
    UAnimMontage* ResolveComboHitReactionMontage(int32 ComboIndex, FName& OutSource) const;

    //───────── 사운드 ─────────
protected:
    // 각 Enemy가 오버라이드할 함수
    virtual void CacheSoundsFromDataAsset();
    
    // 사운드 재생 헬퍼
    void PlayEnemySound(const TWeakObjectPtr<USoundBase>& Sound, float VolumeMultiplier = 1.0f);
    
    // 캐싱된 사운드들
    UPROPERTY(Transient)
    TWeakObjectPtr<USoundBase> CachedAttackSound;
    
    UPROPERTY(Transient)
    TWeakObjectPtr<USoundBase> CachedHitSound;
    
    UPROPERTY(Transient)
    TWeakObjectPtr<USoundBase> CachedDeathSound;

private:
    void RegisterForPlayerRespawnEvents();
    void UnregisterFromPlayerRespawnEvents();
    
    FDelegateHandle PlayerRespawnDelegateHandle;
};