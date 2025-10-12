// Source/PosisonFrog/00_Character/01_Enemy/CTankerBrute.cpp
#include "CTankerBrute.h"
#include "00_Character/02_Component/01_EnemyComponent/CTankerChargeComponent.h"
#include "AIController.h"
#include "GameFramework/CharacterMovementComponent.h"

ACTankerBrute::ACTankerBrute()
{
    ChargeComp = CreateDefaultSubobject<UCTankerChargeComponent>(TEXT("ChargeComp"));
    // 커맨드 띄우기 면역 식별 태그
    Tags.AddUnique(TEXT("Enemy.Type.Tank"));
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
}

void ACTankerBrute::DoChase()
{
    // PreCharge/Windup/Charging 중이면 상위 FSM 로직 일시 중지
    if (ChargeComp && ChargeComp->IsChargingOrWindup())
        return;

    // 쿨다운 중이면 일반 추격
    if (ChargeComp && ChargeComp->IsOnCooldown())
    {
        ACEnemyCharacterBase::DoChase();
        return;
    }

    // 돌진 우선
    if (bPreferCharge && Target && HasVisualOnTarget())
    {
        if (ChargeComp && ChargeComp->RequestCharge(Target.Get()))
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

void ACTankerBrute::HandleChargeStateChanged(EChargeState NewState, EChargeState /*PrevState*/)
{
    // 사운드/FX/상태표시 등 필요 시 구현
    if (NewState == EChargeState::Charging)
    {
        // 예: 카메라 쉐이크, 머티리얼 틴트 등
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
}
