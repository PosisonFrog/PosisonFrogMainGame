#include "CTankerBrute.h"

#include "AIController.h"
#include "00_Character/02_Component/01_EnemyComponent/CTankerChargeComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"       
#include "Components/CapsuleComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Sound/SoundBase.h"

namespace TankerBrute
{
    const FName AttackEffectSocketName = TEXT("SignSocket");
}

using namespace TankerBrute;

ACTankerBrute::ACTankerBrute()
{
    ChargeComp = CreateDefaultSubobject<UCTankerChargeComponent>(TEXT("ChargeComp"));

    Tags.AddUnique(TEXT("Enemy.Type.Tank"));
    SightDistance = FMath::Max(SightDistance, ChargeStopDistanceOverride);
    ChaseStartDistance = FMath::Max(ChaseStartDistance, SightDistance);

 
  if (UCapsuleComponent* Capsule = GetCapsuleComponent())
  {
      const float DesiredSeparation = Capsule->GetScaledCapsuleRadius() * 2.f + 5.f;
      SeparationRadius = FMath::Max(SeparationRadius, DesiredSeparation);
  }   
}

void ACTankerBrute::PostInitProperties()
{
    Super::PostInitProperties();
    SyncAttackTuning();
}

void ACTankerBrute::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    SyncAttackTuning();
}

void ACTankerBrute::BeginPlay()
{
    Super::BeginPlay();

    InitialiseChargeComponent();
}

void ACTankerBrute::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (ChargeStopOverrideRestoreTime < 0.f)
    {
        return;
    }
    
    const float Now = GetWorld()->GetTimeSeconds();
    if (Target)
    {
        LastSeenTime = Now;
    }
    
    const bool bCharging = ChargeComp && ChargeComp->IsChargingOrWindup();
    if (bCharging)
    {
        return;
    }
    
    UpdateChargeStopOverride(Now);
}


bool ACTankerBrute::HasVisualOnTarget() const
{
    if (!Target)
    {
        return false;
    }
    
    const float DistSq = FVector::DistSquared(GetActorLocation(), Target->GetActorLocation());
    return DistSq <= FMath::Square(SightDistance);
}

void ACTankerBrute::DoChase()
{
    // PreCharge/Windup/Charging 중이면 상위 FSM 로직 일시 중지
    if (ChargeComp && ChargeComp->IsChargingOrWindup())
    {
        return;
    }
    
    // 돌진 우선
    if (TryStartCharge())
    {
        return; // 컴포넌트가 이후 전이 주도
    }

    // 기본 추격
    Super::DoChase();
}

void ACTankerBrute::DoAttack()
{
    if (!Target)
    {
        SetState(EEnemyState::ReturnHome);
        return;
    }

    if (ChargeComp && ChargeComp->IsChargingOrWindup())
    {
        bIsPerformingMelee = false;
        return;
    }
    
    if (TryStartCharge())
    {
        bIsPerformingMelee = false;
        LastSeenTime = GetWorld()->GetTimeSeconds();
    }

    const float Dist = DistToTarget();
    const float ExitDistance = FMath::Max(AttackExitDistance, MeleeAttackDistance * 1.1f);
    if (Dist > ExitDistance)
    {
        bIsPerformingMelee = false;
        SetState(EEnemyState::Chase);
        return;
    }

    const float DesiredRange = FMath::Max(MeleeAttackDistance, AttackRange);
    if (Dist > DesiredRange * 0.9f)
    {
        if (bUseNavigation)
        {
            RequestMoveTo(Target->GetActorLocation(), AttackMoveAcceptanceRadius);
        }
        else
        {
            FVector Dir = Target->GetActorLocation() - GetActorLocation();
            Dir.Z = 0.f;
            if (Dir.Normalize())
            {
                AddMovementInput(Dir, 1.f);
            }
        }
    }
    else
    {
        StopMove();
    }

    
    if (!bIsPerformingMelee && IsAttackReady() && Dist <= MeleeAttackDistance)
    {
        StartAttack();
    }
}

void ACTankerBrute::OnDead()
{
    Super::OnDead();

    if (USkeletalMeshComponent* MeshComp = GetMesh())
    {
        if (UAnimInstance* Anim = MeshComp->GetAnimInstance())
        {
            Anim->Montage_Play(DeadMontage, 1.0f);
        }
    }
    
    if (HitEffect)
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(this,HitEffect,GetActorLocation(), GetActorRotation());
    }
        
    if (HitSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, HitSound, GetActorLocation());
    }
}



void ACTankerBrute::StartAttack()
{
    if (!Target) return;

    LastAttackTime = GetWorld()->GetTimeSeconds();
    bIsPerformingMelee = true;
    AttackStartedTime = GetWorld()->GetTimeSeconds();

    
    StopMovement();
    SpawnAttackEffect();
    PlayMontageIfValid(AttackMontage);
    PlaySoundIfValid(AttackSound);
    
    GetWorldTimerManager().SetTimer(
        Timer_WindUp,
        this,
        &ACTankerBrute::BeginAttackWindow,
        AttackWindUpTime,
        false);
}

void ACTankerBrute::BeginAttackWindow()
{
    // Base의 분할 스윕 시스템 사용: 창을 열고, 시작 시 즉시 1회 판정
    AttackWindowBegin(AttackActiveWindow);
    ApplyAttackDamage(/*bCheckAngle=*/true);
    SpawnHitEffectAtForward();
    
    FTimerManager& TimerManager = GetWorldTimerManager();
    const FTimerDelegate EndWindowDelegate = FTimerDelegate::CreateUObject(this, &ACTankerBrute::EndAttackWindow, false);
    TimerManager.SetTimer(Timer_EndWindow, EndWindowDelegate, AttackActiveWindow, false);

    const float FinishDelay = AttackActiveWindow + AttackRecoveryTime;
    TimerManager.SetTimer(Timer_Finish, this, &ACTankerBrute::FinishAttack, FinishDelay, false);
 
    if (bDebugAttackLog)
    {
        UE_LOG(LogTemp, Log, TEXT("[Riot] Attack window opened (%.2fs)"), AttackActiveWindow);
    }
}

void ACTankerBrute::EndAttackWindow(bool bForced)
{
    AttackWindowEnd(bForced);
    GetWorldTimerManager().ClearTimer(Timer_EndWindow);
}

void ACTankerBrute::FinishAttack()
{
    bIsAttacking = false;
    
    ClearAttackTimers();
 
    if (bDebugAttackLog)
    {
        UE_LOG(LogTemp, Verbose, TEXT("[Riot] Attack finished"));
    }
}

void ACTankerBrute::CancelAttack()
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
void ACTankerBrute::ExitState(EEnemyState OldState)
{
    Super::ExitState(OldState);
    if (OldState == EEnemyState::Attack)
        CancelAttack();
}


void ACTankerBrute::PlayMontageIfValid(UAnimMontage* Montage, float PlayRate) const
{
    if (!Montage)
        return;

    if (USkeletalMeshComponent* MeshComp = GetMesh())
    {
        if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
        {
            AnimInstance->Montage_Play(Montage, PlayRate);
        }
    }
}



void ACTankerBrute::PlaySoundIfValid(USoundBase* Sound) const
{
    if (Sound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation());
    }
}

void ACTankerBrute::SpawnAttackEffect() const
{
    if (!AttackEffect)
        return;

    if (USkeletalMeshComponent* MeshComp = GetMesh())
    {
        UNiagaraFunctionLibrary::SpawnSystemAttached(
            AttackEffect,
            MeshComp,
            AttackEffectSocketName,
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            EAttachLocation::SnapToTarget,
            true);
    }
}

void ACTankerBrute::SpawnHitEffectAtForward() const
{
    if (!HitEffect)
        return;

    const FVector SpawnLocation = GetActorLocation() + GetActorForwardVector() * 100.f;
    UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, HitEffect, SpawnLocation, GetActorRotation());
}

void ACTankerBrute::SpawnHitEffectAtLocation() const
{
    if (!HitEffect)
        return;

    UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, HitEffect, GetActorLocation(), GetActorRotation());
}

AAIController* ACTankerBrute::GetEnemyAIController() const
{
    return Cast<AAIController>(GetController());
}

void ACTankerBrute::StopMovement() const
{
    if (AAIController* AI = GetEnemyAIController())
    {
        AI->StopMovement();
    }
}

void ACTankerBrute::StopMovementAndFaceTarget()
{
    StopMovement();

    if (!Target)
        return;

    
    const FRotator LookAt = UKismetMathLibrary::FindLookAtRotation(GetActorLocation(), Target->GetActorLocation());
    SetActorRotation(FRotator(0.f, LookAt.Yaw, 0.f));
}

void ACTankerBrute::ClearAttackTimers()
{
    FTimerManager& TimerManager = GetWorldTimerManager();
    TimerManager.ClearTimer(Timer_WindUp);
    TimerManager.ClearTimer(Timer_EndWindow);
    TimerManager.ClearTimer(Timer_Finish);
}

void ACTankerBrute::InitialiseChargeComponent()
{
    if (!ensure(ChargeComp))
    {
        return;
    }
    
    ChargeComp->OnChargeStateChanged.AddDynamic(this, &ACTankerBrute::HandleChargeStateChanged);
    ChargeComp->OnChargeFinished.AddDynamic(this, &ACTankerBrute::HandleChargeFinished);
}

bool ACTankerBrute::ShouldAttemptCharge() const
{
    return bPreferCharge
        && ChargeComp
        && Target
        && !ChargeComp->IsOnCooldown()
        && HasVisualOnTarget();
}

bool ACTankerBrute::TryStartCharge()
{
    if (!ShouldAttemptCharge())
    {
        return false;
    }
    
    if (!ChargeComp->RequestCharge(Target.Get()))
    {
        return false;
    }
    
    LastSeenTime = GetWorld()->GetTimeSeconds();
    return true;
}

void ACTankerBrute::UpdateChargeStopOverride(float CurrentTime)
{
    if (CurrentTime < ChargeStopOverrideRestoreTime)
    {
        return;
    }
    
    if (CachedChaseStopDistance >= 0.f)
    {
        ChaseStopDistance = CachedChaseStopDistance;
    }
    
    ChargeStopOverrideRestoreTime = -1.f;
}


void ACTankerBrute::HandleImmediatePostCharge(float CurrentTime)
{
    LastChargeFinishedTime = CurrentTime;
    LastSeenTime = CurrentTime;
    
    if (PostChargeChaseGraceTime > 0.f)
    {
        ChargeStopOverrideRestoreTime = CurrentTime + PostChargeChaseGraceTime;
    }
    else
    {
        
        ChargeStopOverrideRestoreTime = CurrentTime;
    }
}
    

void ACTankerBrute::HandleChargeStateChanged(EChargeState NewState, EChargeState /*PrevState*/)
{
    // 사운드/FX/상태표시 등 필요 시 구현
    if (NewState == EChargeState::PreCharge || NewState == EChargeState::Windup || NewState == EChargeState::Charging)
    {
        if (CachedChaseStopDistance < 0.f)
        {
            CachedChaseStopDistance = ChaseStopDistance;
        }
           
        ChaseStopDistance = FMath::Max(ChaseStopDistance, ChargeStopDistanceOverride);
        ChargeStopOverrideRestoreTime = -1.f;
            
        LastSeenTime = GetWorld()->GetTimeSeconds();
    }
}


void ACTankerBrute::HandleChargeFinished(EChargeEndReason Reason, AActor* HitActor)
{
    // 돌진 종료 → 상위 FSM 정상 복귀
    if (State != EEnemyState::Dead)
    {
        if (Target)
            SetState(EEnemyState::Chase);
        else
            SetState(EEnemyState::ReturnHome);
    }
    const float Now = GetWorld()->GetTimeSeconds();
    HandleImmediatePostCharge(Now);
}

void ACTankerBrute::SyncAttackTuning()
{
    AttackInterval = AttackIntervalTanker;
    AttackRange = AttackRangeTanker;
    BaseDamage = AttackDamage;
}
