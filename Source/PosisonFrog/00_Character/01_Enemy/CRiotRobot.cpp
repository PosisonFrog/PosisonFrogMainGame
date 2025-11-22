#include "CRiotRobot.h"
 
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "NiagaraFunctionLibrary.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "Global.h"
 
#include "01_AIController/CTacticalEnemyAIController.h"  // 전술 컨트롤러
#include "05_System/01_Sound/CSoundManagerSubsystem.h"
#include "05_System/01_Sound//CSoundDataAsset.h"
#include "00_Character/CMainGameModeBase.h"

namespace RiotRobot
{
    constexpr float TacticalRingPadding = 80.f;
    constexpr float TacticalStrafeOffset = 60.f;
    constexpr float TacticalStrafeAngularSpeed = 120.f;
    constexpr float TacticalStrafeDuration = 0.6f;
    constexpr float TacticalStrafeAcceleration = 100.f;
    const FName AttackEffectSocketName(TEXT("SignSocket"));
}

using namespace RiotRobot;

// ─────────────────────────────────────────────────────
// 생성/초기화
// ─────────────────────────────────────────────────────
ACRiotRobot::ACRiotRobot()
{
    // 전술 컨트롤러 사용(Detour Crowd 기반)
    AIControllerClass = ACTacticalEnemyAIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

    // 기본 공격/물리 튜닝
    AttackIntervalRiot = 1.0f;
    AttackRangeRiot    = 200.f;
    AttackWindUpTime   = 0.53f;
    AttackActiveWindow = 0.65f;
    AttackRecoveryTime = 1.35f;
    AttackDamage       = 5.5f;

    CapsuleLinearDamping  = 0.5f;
    CapsuleAngularDamping = 0.5f;

    // Base에 있는 기본 값과 동기화
    AttackInterval      = AttackIntervalRiot;
    BaseDamage          = AttackDamage;
    AttackEnterDistance = 160.f;
    AttackExitDistance  = 220.f;

    if (USkeletalMeshComponent* MeshComp = GetMesh())
    {
        MeshComp->SetGenerateOverlapEvents(false);
        MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

        MeshComp->SetCollisionResponseToAllChannels(ECR_Overlap);
        MeshComp->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);
        MeshComp->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Ignore);
    }
    
    PrimaryActorTick.bCanEverTick = true; // Base의 Tick을 그대로 사용(전술은 컨트롤러 Tick에서)
}

void ACRiotRobot::PostInitProperties()
{
    Super::PostInitProperties();
    SyncAttackTuning();
}

#if WITH_EDITOR
void ACRiotRobot::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    SyncAttackTuning();
}
#endif

void ACRiotRobot::BeginPlay()
{
    Super::BeginPlay();
    
    // 하위 호환성: HitMontage가 설정되어 있고 ComboHitReactionMontages가 비어있으면
    // 모든 콤보 인덱스에 동일한 몽타주 할당
    if (HitMontage && ComboHitReactionMontages.Num() == 0)
    {
        ComboHitReactionMontages.SetNum(3);
        for (int32 i = 0; i < 3; ++i)
        {
            ComboHitReactionMontages[i] = HitMontage;
        }
        UE_LOG(LogTemp, Warning, TEXT("[RiotRobot] Auto-migrated HitMontage to ComboHitReactionMontages. Please set combo-specific montages in Blueprint."));
    }

    SyncAttackTuning();
    SetupCapsulePhysics();

    TryPlayIdleMontage();
}

void ACRiotRobot::SetupCapsulePhysics()
{
    if (UCapsuleComponent* CapsuleComp = GetCapsuleComponent())
    {
        CapsuleComp->SetCollisionObjectType(PF::Collision::RiotEnemy);
        CapsuleComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

        CapsuleComp->SetLinearDamping(CapsuleLinearDamping);
        CapsuleComp->SetAngularDamping(CapsuleAngularDamping);
        
        CapsuleComp->SetCollisionResponseToAllChannels(ECR_Block);
        CapsuleComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
        CapsuleComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
        CapsuleComp->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Overlap);
        CapsuleComp->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Overlap);
        // 상세 마찰은 물리재질(Physical Material)에서 별도 관리 권장
    }
}

// ─────────────────────────────────────────────────────
// FSM 오버라이드: 추적/공격
// ─────────────────────────────────────────────────────

void ACRiotRobot::DoChase()
{
    // 공격 거리 이내면 Attack 상태로 전환(부모 로직과 동일 판단)
    if (!Target)
    {
        SetState(EEnemyState::ReturnHome);
        return;
    }

    const float Dist = DistToTarget();

    if (Dist >= ChaseStopDistance || (!HasVisualOnTarget() && GetWorld()->GetTimeSeconds() - LastSeenTime >= LoseSightGrace))
    {
        SetState(EEnemyState::ReturnHome);
        return;
    }

    if (HasVisualOnTarget())
    {
        LastSeenTime = GetWorld()->GetTimeSeconds();
    }

    if (ShouldEnterAttackFromChase())
    {
        StopMovement();
        SetState(EEnemyState::Attack);
        return;
    }

    RequestTacticalChase();
}

void ACRiotRobot::DoAttack()
{
    // 사거리 벗어나면 추적으로 복귀
    if (!Target || DistToTarget() >= AttackExitDistance)
    {
        CancelAttack();
        SetState(EEnemyState::Chase);
        return;
    }

    if (!IsGroundedForAttack())
    {
        if (bIsAttacking)
        {
            CancelAttack();
        }
        return;
    }
    
    if (bIsAttacking)
    {
        StopMovement();  // 매 프레임 정지 명령
        return;  // 공격 중에는 다른 행동 하지 않음
    }

    // 공격 중이 아니고, 쿨타임이 끝났다면 공격 시작
    if (!bIsAttacking && IsAttackReady())
    {
        StartAttack();
        return;
    }

    HandleCooldownStrafe();
}

// ─────────────────────────────────────────────────────
// 공격 흐름(타이머 기반 스윙 창)
// ─────────────────────────────────────────────────────

void ACRiotRobot::StartAttack()
{
    if (!Target) return;

    if (!IsGroundedForAttack())
        return;
    
    LastAttackTime = GetWorld()->GetTimeSeconds();
    bIsAttacking = true;
    AttackStartedTime = GetWorld()->GetTimeSeconds();

    StopMovementAndFaceTarget();
    
    //공격 몽타주 재생 추가
    PlayMontageIfValid(AttackMontage);
    
    //공격 사운드 재생 추가
    //PlayEnemySound(CachedAttackSound, 1.0f);
    
    SpawnAttackEffect();
    
    GetWorldTimerManager().SetTimer(
        Timer_WindUp,
        this,
        &ACRiotRobot::BeginAttackWindow,
        AttackWindUpTime,
        false);
}

void ACRiotRobot::BeginAttackWindow()
{
    // Base의 분할 스윕 시스템 사용: 창을 열고, 시작 시 즉시 1회 판정
    AttackWindowBegin(AttackActiveWindow);
    ApplyAttackDamage(/*bCheckAngle=*/true);
    SpawnHitEffectAtForward();
    
    FTimerManager& TimerManager = GetWorldTimerManager();
    const FTimerDelegate EndWindowDelegate = FTimerDelegate::CreateUObject(this, &ACRiotRobot::EndAttackWindow, false);
    TimerManager.SetTimer(Timer_EndWindow, EndWindowDelegate, AttackActiveWindow, false);

    const float FinishDelay = AttackWindUpTime + AttackActiveWindow + AttackRecoveryTime;
    TimerManager.SetTimer(Timer_Finish, this, &ACRiotRobot::FinishAttack, FinishDelay, false);
 
    if (bDebugAttackLog)
    {
        UE_LOG(LogTemp, Log, TEXT("[Riot] Attack window opened (%.2fs)"), AttackActiveWindow);
    }
}

void ACRiotRobot::EndAttackWindow(bool bForced /*=true*/)
{
    AttackWindowEnd(bForced);
    GetWorldTimerManager().ClearTimer(Timer_EndWindow);
}

void ACRiotRobot::FinishAttack()
{
    bIsAttacking = false;

    TryPlayIdleMontage();
    ClearAttackTimers();
 
    if (bDebugAttackLog)
    {
        UE_LOG(LogTemp, Verbose, TEXT("[Riot] Attack finished"));
    }
}

void ACRiotRobot::CancelAttack()
{
    const bool bWasAttacking = bIsAttacking;
 
    if (bWasAttacking)
    {
        EndAttackWindow(true);
        bIsAttacking = false;
    }
 
    ClearAttackTimers();
 
    if (bDebugAttackLog && bWasAttacking)
    {
        UE_LOG(LogTemp, Verbose, TEXT("[Riot] Attack canceled"));
    }
}

// Attack 상태에서 벗어날 때(Chase 등으로 전환 시) 안전하게 타이머 정리
void ACRiotRobot::ExitState(EEnemyState OldState)
{
    Super::ExitState(OldState);
    if (OldState == EEnemyState::Attack)
        CancelAttack();
}

// ─────────────────────────────────────────────────────
// 사망/연출
// ─────────────────────────────────────────────────────
void ACRiotRobot::OnDead()
{
    CancelAttack();
    Super::OnDead();
 
    PlayMontageIfValid(DeadMontage);
    SpawnHitEffectAtLocation();
    //PlayEnemySound(CachedDeathSound, 1.0f);
}

// ─────────────────────────────────────────────────────
// 헬퍼
// ─────────────────────────────────────────────────────
AAIController* ACRiotRobot::GetEnemyAIController() const
{
    return Cast<AAIController>(GetController());
}

ACTacticalEnemyAIController* ACRiotRobot::GetTacticalController() const
{
    return Cast<ACTacticalEnemyAIController>(GetController());
}

void ACRiotRobot::StopMovement() const
{
    if (ACTacticalEnemyAIController* Tactical = GetTacticalController())
    {
        Tactical->StopMovement();
        // 만약 Tactical Controller에 별도의 정지 함수가 있다면
        // Tactical->CancelTacticalMovement(); // 예시
    }
    else if (AAIController* AI = GetEnemyAIController())
    {
        AI->StopMovement();
    }
}

void ACRiotRobot::StopMovementAndFaceTarget()
{
    StopMovement();

    if (!Target)
        return;
    
    const FRotator LookAt = UKismetMathLibrary::FindLookAtRotation(GetActorLocation(), Target->GetActorLocation());
    SetActorRotation(FRotator(0.f, LookAt.Yaw, 0.f));
}

void ACRiotRobot::RequestTacticalChase()
{
    if (!Target)
        return;

    if (bIsAttacking)
        return;

    if (ACTacticalEnemyAIController* Tactical = GetTacticalController())
    {
        const float RingRadius = AttackEnterDistance + TacticalRingPadding;
        Tactical->TacticalChaseRing(Target, RingRadius, PatrolPointReachRadius, /*focus*/true);
        return;
    }
 
    // 콜리전 비활성화 (선택사항)
    RequestMoveTo(Target->GetActorLocation(), PatrolPointReachRadius);
}
 
bool ACRiotRobot::ShouldEnterAttackFromChase() const
{
    return Target && DistToTarget() <= AttackEnterDistance && HasVisualOnTarget();
}

void ACRiotRobot::HandleCooldownStrafe()
{
    if (!Target || bIsAttacking || IsAttackReady())
        return;

    if (ACTacticalEnemyAIController* Tactical = GetTacticalController())
    {
        const float Sign = (FMath::FRand() > 0.5f) ? 1.f : -1.f;
        Tactical->TacticalStrafe(Target,
                                 AttackEnterDistance + TacticalStrafeOffset,
                                 Sign * TacticalStrafeAngularSpeed,
                                 TacticalStrafeDuration,
                                 TacticalStrafeAcceleration);
    }
}

void ACRiotRobot::TryPlayIdleMontage() const
{
    PlayMontageIfValid(IdleMontage);
}

void ACRiotRobot::SpawnAttackEffect() const
{
    /*if (!TakeDamageNormal_VFX)
        return;

    if (USkeletalMeshComponent* MeshComp = GetMesh())
    {
        UNiagaraFunctionLibrary::SpawnSystemAttached(
            TakeDamageNormal_VFX,
            MeshComp,
            RiotRobot::AttackEffectSocketName,
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            EAttachLocation::SnapToTarget,
            true);
    }*/
}

void ACRiotRobot::SpawnHitEffectAtForward() const
{
    if (!HitEffect)
        return;

    const FVector SpawnLocation = GetActorLocation() + GetActorForwardVector() * 100.f;
    UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, HitEffect, SpawnLocation, GetActorRotation());
}

void ACRiotRobot::SpawnHitEffectAtLocation() const
{
    if (!HitEffect)
        return;

    UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, HitEffect, GetActorLocation(), GetActorRotation());
}

void ACRiotRobot::ClearAttackTimers()
{
    FTimerManager& TimerManager = GetWorldTimerManager();
    TimerManager.ClearTimer(Timer_WindUp);
    TimerManager.ClearTimer(Timer_EndWindow);
    TimerManager.ClearTimer(Timer_Finish);
}

void ACRiotRobot::SyncAttackTuning()
{
    AttackInterval = AttackIntervalRiot;
    AttackRange = AttackRangeRiot;
    BaseDamage = AttackDamage;
}

bool ACRiotRobot::IsGroundedForAttack() const
{
    if (const UCharacterMovementComponent* MovementComp = GetCharacterMovement())
    {
        return !MovementComp->IsFalling();
    }
    return true;
}


