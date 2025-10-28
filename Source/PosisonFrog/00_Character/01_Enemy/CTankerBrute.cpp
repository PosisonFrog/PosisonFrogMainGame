#include "CTankerBrute.h"

#include "00_Character/02_Component/01_EnemyComponent/CTankerChargeComponent.h"
#include "AIController.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"       
#include "Sound/SoundBase.h"


ACTankerBrute::ACTankerBrute()
{
    ChargeComp = CreateDefaultSubobject<UCTankerChargeComponent>(TEXT("ChargeComp"));

    Tags.AddUnique(TEXT("Enemy.Type.Tank"));
    SightDistance = FMath::Max(SightDistance, ChargeStopDistanceOverride);
    ChaseStartDistance = FMath::Max(ChaseStartDistance, SightDistance);
     
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
        bIsPerformingMelee = true;
        LastAttackTime = GetWorld()->GetTimeSeconds();
        
        ApplyAttackDamage();

        EnsureWalkingAndResume();
        ReengageChase(AttackReengageDelay);
        bIsPerformingMelee = false;
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
