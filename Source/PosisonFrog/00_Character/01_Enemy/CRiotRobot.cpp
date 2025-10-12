#include "CRiotRobot.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "NiagaraFunctionLibrary.h"       
#include "Sound/SoundBase.h"

#include "01_AIController/CTacticalEnemyAIController.h"  // 전술 컨트롤러
#include "00_Character/00_Player/CPlayerCharacter.h"

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
    AttackWindUpTime   = 0.25f;
    AttackActiveWindow = 0.22f;
    AttackRecoveryTime = 0.35f;
    AttackDamage       = 15.f;

    CapsuleLinearDamping  = 0.5f;
    CapsuleAngularDamping = 0.5f;

    // Base에 있는 기본 값과 동기화
    AttackInterval   = AttackIntervalRiot;
    BaseDamage       = AttackDamage;
    AttackEnterDistance = 160.f;
    AttackExitDistance  = 220.f;

    PrimaryActorTick.bCanEverTick = true; // Base의 Tick을 그대로 사용(전술은 컨트롤러 Tick에서)
}

void ACRiotRobot::BeginPlay()
{
    Super::BeginPlay();
    SetupCapsulePhysics();

    // 대기 몽타주가 있으면 루프 재생(선택)
    if (IdleMontage)
    {
        if (USkeletalMeshComponent* mesh = GetMesh())
            if (UAnimInstance* Anim = mesh->GetAnimInstance())
                Anim->Montage_Play(IdleMontage, 1.0f);
    }
}

void ACRiotRobot::SetupCapsulePhysics()
{
    if (UCapsuleComponent* Capsule = GetCapsuleComponent())
    {
        Capsule->SetLinearDamping(CapsuleLinearDamping);
        Capsule->SetAngularDamping(CapsuleAngularDamping);
        Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
        Capsule->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
        Capsule->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
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

    if (Dist <= AttackEnterDistance && HasVisualOnTarget())
    {
        if (AAIController* AI = Cast<AAIController>(GetController()))
            AI->StopMovement();

        SetState(EEnemyState::Attack);
        return;
    }

    // ─ 전술 컨트롤러로 링 포위 추적 ─
    if (AAIController* AIC = Cast<AAIController>(GetController()))
    {
        if (ACTacticalEnemyAIController* TAC = Cast<ACTacticalEnemyAIController>(AIC))
        {
            const float RingRadius = AttackEnterDistance + 80.f; // 약간의 패딩
            TAC->TacticalChaseRing(Target, RingRadius, /*acc*/PatrolPointReachRadius, /*focus*/true);
            return;
        }
    }

    // NavMesh가 없거나 전술컨이 아니라면 기본 이동
    RequestMoveTo(Target->GetActorLocation(), PatrolPointReachRadius);
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

    // 공격 중이 아니고, 쿨타임이 끝났다면 공격 시작
    if (!bIsAttacking && IsAttackReady())
    {
        StartAttack();
        return;
    }

    // 쿨타임/후딜 동안 타깃을 중심으로 짧게 스트레이프하여 줄서기/경로끼임 완화
    if (bIsAttacking == false && !IsAttackReady())
    {
        if (ACTacticalEnemyAIController* TAC = Cast<ACTacticalEnemyAIController>(GetController()))
        {
            const float Sign = (FMath::FRand() > 0.5f) ? +1.f : -1.f;
            TAC->TacticalStrafe(Target, AttackEnterDistance + 60.f, /*deg/s*/Sign*120.f,
                                /*duration*/0.6f, /*acc*/100.f);
        }
    }
}

// ─────────────────────────────────────────────────────
// 공격 흐름(타이머 기반 스윙 창)
// ─────────────────────────────────────────────────────

void ACRiotRobot::StartAttack()
{
    if (!Target) return;

    bIsAttacking = true;
    AttackStartedTime = GetWorld()->GetTimeSeconds();

    USkeletalMeshComponent* CharMesh = GetMesh();
    // Niagara 이펙트 스폰
    if (AttackEffect && CharMesh)
    {
        UNiagaraFunctionLibrary::SpawnSystemAttached(
            AttackEffect,
            CharMesh,
            TEXT("SignSocket"), 
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            EAttachLocation::SnapToTarget,
            true 
        );
    }
    
    // 실제 공격 시작 시점에 쿨타임 갱신
    LastAttackTime = AttackStartedTime;

    // 이동 정지(히트 안정화)
    if (AAIController* AI = Cast<AAIController>(GetController()))
        AI->StopMovement();

    // 타깃을 바라봄
    const FRotator LookAt = UKismetMathLibrary::FindLookAtRotation(GetActorLocation(), Target->GetActorLocation());
    SetActorRotation(FRotator(0.f, LookAt.Yaw, 0.f));

    // 공격 모션/사운드
    if (AttackMontage)
    {
        if (USkeletalMeshComponent* mesh = GetMesh())
            if (UAnimInstance* Anim = mesh->GetAnimInstance())
                Anim->Montage_Play(AttackMontage, 1.0f);
    }
    if (AttackSound)
        UGameplayStatics::PlaySoundAtLocation(this, AttackSound, GetActorLocation());

    // ─ 윈드업 후 스윙 창 열기 ─
    GetWorldTimerManager().SetTimer(Timer_WindUp, this, &ACRiotRobot::BeginAttackWindow, AttackWindUpTime, false);
}

void ACRiotRobot::BeginAttackWindow()
{
    // Base의 분할 스윕 시스템 사용: 창을 열고, 시작 시 즉시 1회 판정
    AttackWindowBegin(AttackActiveWindow);
    ApplyAttackDamage(/*bCheckAngle=*/true);

    // Niagara 이펙트 스폰
    if (HitEffect)
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            this,
            HitEffect,
            GetActorLocation() + GetActorForwardVector() * 100.f,
            GetActorRotation()
        );
    }

    // 스윙 창 자동 종료 타이머(안전)
    GetWorldTimerManager().SetTimer(Timer_EndWindow, FTimerDelegate::CreateUObject(
        this, &ACRiotRobot::EndAttackWindow, true), AttackActiveWindow, false);

    // 리커버리 종료 → FinishAttack
    const float FinishDelay = AttackActiveWindow + AttackRecoveryTime;
    GetWorldTimerManager().SetTimer(Timer_Finish, this, &ACRiotRobot::FinishAttack, FinishDelay, false);

    if (bDebugAttackLog)
        UE_LOG(LogTemp, Log, TEXT("[Riot] Attack window opened (%.2fs)"), AttackActiveWindow);
}

void ACRiotRobot::EndAttackWindow(bool bForced /*=true*/)
{
    AttackWindowEnd(/*bForce*/bForced);
    GetWorldTimerManager().ClearTimer(Timer_EndWindow);
}

void ACRiotRobot::FinishAttack()
{
    bIsAttacking = false;

    
    // 대기 모션(선택)
    if (IdleMontage)
    {
        if (USkeletalMeshComponent* mesh = GetMesh())
            if (UAnimInstance* Anim = mesh->GetAnimInstance())
                Anim->Montage_Play(IdleMontage, 1.0f);
    }

    // 타이머 클린업
    GetWorldTimerManager().ClearTimer(Timer_WindUp);
    GetWorldTimerManager().ClearTimer(Timer_EndWindow);
    GetWorldTimerManager().ClearTimer(Timer_Finish);

    if (bDebugAttackLog)
        UE_LOG(LogTemp, Verbose, TEXT("[Riot] Attack finished"));
}

void ACRiotRobot::CancelAttack()
{
    if (!bIsAttacking) return;

    EndAttackWindow(true);
    bIsAttacking = false;

    GetWorldTimerManager().ClearTimer(Timer_WindUp);
    GetWorldTimerManager().ClearTimer(Timer_EndWindow);
    GetWorldTimerManager().ClearTimer(Timer_Finish);

    if (bDebugAttackLog)
        UE_LOG(LogTemp, Verbose, TEXT("[Riot] Attack canceled"));
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

    // 이동 비활성화
    GetCharacterMovement()->DisableMovement();
    GetCharacterMovement()->StopMovementImmediately();

    // 콜리전 비활성화 (선택사항)
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // 공격 모션/사운드
    if (DeadMontage)
    {
        if (USkeletalMeshComponent* mesh = GetMesh())
            if (UAnimInstance* Anim = mesh->GetAnimInstance())
                Anim->Montage_Play(DeadMontage, 1.0f);
    }

    
    
    // Niagara 이펙트 스폰
    if (HitEffect)
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(this,HitEffect,GetActorLocation(),GetActorRotation());
    if (HitSound)
        UGameplayStatics::PlaySoundAtLocation(this, HitSound, GetActorLocation());
}