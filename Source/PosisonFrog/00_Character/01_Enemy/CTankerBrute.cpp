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
    // (옵션) 기본 근접 공격
    if (IsAttackReady() && DistToTarget() <= MeleeAttackDistance)
    {
        LastAttackTime = GetWorld()->GetTimeSeconds();
        ApplyAttackDamage(); // 베이스의 캡슐 스윕 판정 사용
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
