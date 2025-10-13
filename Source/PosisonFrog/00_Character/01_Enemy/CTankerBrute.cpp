// Source/PosisonFrog/00_Character/01_Enemy/CTankerBrute.cpp
#include "CTankerBrute.h"
#include "00_Character/02_Component/01_EnemyComponent/CTankerChargeComponent.h"
#include "AIController.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NiagaraFunctionLibrary.h"       
#include "Sound/SoundBase.h"
#include "Components/SkeletalMeshComponent.h"


ACTankerBrute::ACTankerBrute()
{
    ChargeComp = CreateDefaultSubobject<UCTankerChargeComponent>(TEXT("ChargeComp"));
    // 커맨드 띄우기 면역 식별 태그
    Tags.AddUnique(TEXT("Enemy.Type.Tank"));
    SightDistance      = FMath::Max(SightDistance, ChargeStopDistanceOverride);
    ChaseStartDistance = FMath::Max(ChaseStartDistance, SightDistance);
     
}

void ACTankerBrute::BeginPlay()
{
    Super::BeginPlay();

    if (ensure(ChargeComp))
    {
        ChargeComp->OnChargeStateChanged.AddDynamic(this, &ACTankerBrute::HandleChargeStateChanged);
        ChargeComp->OnChargeFinished   .AddDynamic(this, &ACTankerBrute::HandleChargeFinished);
    }
}

void ACTankerBrute::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (ChargeStopOverrideRestoreTime >= 0.f)
    {
        const float Now = GetWorld()->GetTimeSeconds();

        if (Target)
        {
            // 돌진 직후 시야 판정을 최근으로 유지해 추격 상태를 보존한다.
            LastSeenTime = Now;
        }

        const bool bCharging = ChargeComp && ChargeComp->IsChargingOrWindup();
        if (!bCharging && Now >= ChargeStopOverrideRestoreTime)
        {
            if (CachedChaseStopDistance >= 0.f)
            {
                ChaseStopDistance = CachedChaseStopDistance;
            }

            ChargeStopOverrideRestoreTime = -1.f;
        }
    }
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
        return;

    const bool bCanAttemptCharge = bPreferCharge
          && ChargeComp
          && Target
          && !ChargeComp->IsOnCooldown()
          && HasVisualOnTarget();

    // 돌진 우선
    if (bCanAttemptCharge && ChargeComp->RequestCharge(Target.Get()))
    {
        LastSeenTime = GetWorld()->GetTimeSeconds();
        return; // 컴포넌트가 이후 전이 주도
    }

    // 기본 추격
    ACEnemyCharacterBase::DoChase();
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
    
    if (ChargeComp && Target && !ChargeComp->IsOnCooldown() && HasVisualOnTarget())
    {
        if (ChargeComp->RequestCharge(Target.Get()))
        {
            bIsPerformingMelee = false;
            LastSeenTime = GetWorld()->GetTimeSeconds();
            return;
        }
    }
    const float Dist = DistToTarget();

    // 사거리를 충분히 벗어나면 추격 상태로 복귀
    const float ExitDistance = FMath::Max(AttackExitDistance, MeleeAttackDistance * 1.1f);
    if (Dist > ExitDistance)
    {
        bIsPerformingMelee = false;
        SetState(EEnemyState::Chase);
        return;
    }

    // 공격 사거리 확보 전까지는 계속 접근을 유지한다.
    const float DesiredRange = FMath::Max(MeleeAttackDistance, AttackRange);
    if (Dist > DesiredRange * 0.9f)
    {
        if (bUseNavigation)
        {
            RequestMoveTo(Target->GetActorLocation(), AttackMoveAcceptanceRadius);
        }
        else
        {
            FVector Dir = (Target->GetActorLocation() - GetActorLocation());
            Dir.Z = 0.f;
            if (Dir.Normalize())
                AddMovementInput(Dir, 1.f);
        }
    }
    else
    {
        StopMove();
    }

    // 쿨타임이 끝났고 사거리 안이라면 1회 공격을 수행한다.
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

    // 이동 비활성화
    GetCharacterMovement()->DisableMovement();
    GetCharacterMovement()->StopMovementImmediately();

    // 콜리전 비활성화 (선택사항)
   //GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

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

void ACTankerBrute::HandleChargeStateChanged(EChargeState NewState, EChargeState /*PrevState*/)
{
    // 사운드/FX/상태표시 등 필요 시 구현
    if (NewState == EChargeState::Charging)
    {
        // 예: 카메라 쉐이크, 머티리얼 틴트 등
    }

    
    if (NewState == EChargeState::PreCharge
        || NewState == EChargeState::Windup
        || NewState == EChargeState::Charging)
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

void ACTankerBrute::HandleChargeFinished(EChargeEndReason /*Reason*/, AActor* /*Hit*/)
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
    LastChargeFinishedTime = Now;
    LastSeenTime = Now;
    
    if (PostChargeChaseGraceTime > 0.f)
    {
        ChargeStopOverrideRestoreTime = Now + PostChargeChaseGraceTime;
    }
    else
    {
        ChargeStopOverrideRestoreTime = Now;
    }
}
