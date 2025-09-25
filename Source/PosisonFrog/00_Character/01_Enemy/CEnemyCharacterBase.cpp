// CEnemyCharacterBase.cpp
#include "CEnemyCharacterBase.h"

#include "AIController.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "Navigation/PathFollowingComponent.h"


#include "00_Character/02_Component/CHealthComponent.h"
#include "00_Character/00_Player/CPlayerCharacter.h"

ACEnemyCharacterBase::ACEnemyCharacterBase()
{
    PrimaryActorTick.bCanEverTick = true;
    

    if (UCharacterMovementComponent* M = GetCharacterMovement())
    {
        M->MaxWalkSpeed = 360.f;
        M->bUseControllerDesiredRotation = false;
        M->bOrientRotationToMovement = true;
        M->RotationRate = FRotator(0, 420, 0);
    }

    // 적도 공통 HealthComponent 사용(없으면 생성)
    if (!FindComponentByClass<UCHealthComponent>())
    {
        CreateDefaultSubobject<UCHealthComponent>(TEXT("HealthComponent"));
    }
}

void ACEnemyCharacterBase::BeginPlay()
{
    Super::BeginPlay();

    HomeLocation = GetActorLocation();

    if (!bUseNavigation)
    {
        if (UCharacterMovementComponent* M = GetCharacterMovement())
            M->MaxWalkSpeed = DirectMoveSpeed;
    }

    if (UCHealthComponent* HP = FindComponentByClass<UCHealthComponent>())
    {
        HP->OnHealthChanged.AddDynamic(this, &ACEnemyCharacterBase::OnHealthChanged);
    }

    SetState(EEnemyState::Patrol);
}

void ACEnemyCharacterBase::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!bUseNavigation && bDirectMoveActive)
        DirectMoveTick(DeltaSeconds);

    const float Dist = Target ? FVector::Dist(GetActorLocation(), Target->GetActorLocation()) : FLT_MAX;
    const float Interval = (Dist <= NearThinkDistance) ? RichThinkInterval : CheapThinkInterval;

    if (GetWorld()->GetTimeSeconds() >= NextThinkTime)
    {
        NextThinkTime = GetWorld()->GetTimeSeconds() + Interval;
        Think(DeltaSeconds);
    }
    
    if (bShowDebugInfo)
    {
        DebugDrawState();
    }
}

void ACEnemyCharacterBase::Think(float)
{
    AcquireTarget();

    switch (State)
    {
    case EEnemyState::Patrol:     DoPatrol();      break;
    case EEnemyState::Alert:      DoAlert();       break;
    case EEnemyState::Chase:      DoChase();       break;
    case EEnemyState::Attack:     DoAttack();      break;
    case EEnemyState::ReturnHome: DoReturnHome();  break;
    case EEnemyState::Dead:       DoDead();        break;
    }
}

void ACEnemyCharacterBase::EnterState(EEnemyState) {}
void ACEnemyCharacterBase::ExitState(EEnemyState) {}

void ACEnemyCharacterBase::SetState(EEnemyState NewState)
{
    if (State == NewState) return;
    ExitState(State);
    State = NewState;
    StateEnterTime = GetWorld()->GetTimeSeconds();  // 상태 진입 시간 기록
    EnterState(State);
}

// ───────────────── 상태별 동작 ─────────────────

void ACEnemyCharacterBase::DoPatrol()
{
    // Patrol → Alert : LoS && Dist ≤ ChaseStartDistance
    if (Target && HasVisualOnTarget() && DistToTarget() <= ChaseStartDistance)
    {
        SetState(EEnemyState::Alert);
        return;
    }

    // 순찰 목표 없거나 도달 시 새 목표
    if (PatrolGoal.IsNearlyZero() || Reached(PatrolGoal, PatrolPointReachRadius))
    {
        bool bSet = false;
        if (bUseNavigation)
        {
            if (UNavigationSystemV1* NS = UNavigationSystemV1::GetCurrent(GetWorld()))
            {
                FNavLocation NL;
                if (NS->GetRandomPointInNavigableRadius(HomeLocation, PatrolRoamRadius, NL))
                {
                    PatrolGoal = NL.Location; bSet = true;
                }
            }
        }
        if (!bSet)
        {
            // NavMesh 없을 때: 원점 근처 랜덤 2D 오프셋
            const float R = FMath::FRandRange(PatrolRoamRadius * 0.4f, PatrolRoamRadius);
            const float A = FMath::FRandRange(0.f, 2*PI);
            PatrolGoal = HomeLocation + FVector(FMath::Cos(A)*R, FMath::Sin(A)*R, 0.f);
        }
        RequestMoveTo(PatrolGoal, PatrolPointReachRadius);
    }
}

void ACEnemyCharacterBase::DoAlert()
{
    const float AlertDuration = 0.4f;
    const bool bLoS  = Target && HasVisualOnTarget();
    const bool bNear = Target && DistToTarget() <= ChaseStartDistance;

    if (!bLoS || !bNear)
    {
        SetState(EEnemyState::Patrol);
        return;
    }

    if (GetWorld()->GetTimeSeconds() - StateEnterTime >= AlertDuration)
    {
        SetState(EEnemyState::Chase);
    }
}

void ACEnemyCharacterBase::DoChase()
{
    if (!Target)
    {
        SetState(EEnemyState::ReturnHome);
        return;
    }

    const float Dist = DistToTarget();

    // Chase → Attack : 근접 + 시야
    if (Dist <= AttackEnterDistance && HasVisualOnTarget())
    {
        StopMove();
        SetState(EEnemyState::Attack);
        return;
    }

    // Chase 유지/포기
    if (!HasVisualOnTarget())
    {
        const bool bLoseTooLong = (GetWorld()->GetTimeSeconds() - LastSeenTime) >= LoseSightGrace;
        if (bLoseTooLong || Dist >= ChaseStopDistance)
        {
            SetState(EEnemyState::ReturnHome);
            return;
        }
    }
    else
    {
        LastSeenTime = GetWorld()->GetTimeSeconds();
    }

    RequestMoveTo(Target->GetActorLocation(), PatrolPointReachRadius);
}

void ACEnemyCharacterBase::DoAttack()
{
    if (!Target || DistToTarget() >= AttackExitDistance)
    {
        SetState(EEnemyState::Chase);
        return;
    }

    if (IsAttackReady())
    {
        LastAttackTime = GetWorld()->GetTimeSeconds();

        // 공격 애니메이션 재생 (애니메이션 몽타주 사용)
        // PlayAttackMontage(); // 실제 구현은 파생 클래스에서
    }
}

void ACEnemyCharacterBase::DoReturnHome()
{
    if (Reached(HomeLocation, PatrolPointReachRadius))
    {
        SetState(EEnemyState::Patrol);
        return;
    }
    RequestMoveTo(HomeLocation, PatrolPointReachRadius);
}

void ACEnemyCharacterBase::DoDead()
{
    StopMove();
    // 사망 애니메이션 재생
    // PlayDeathMontage(); // 실제 구현은 파생 클래스에서
}

// ───────────────── 조건/헬퍼 ─────────────────

bool ACEnemyCharacterBase::IsAttackReady() const
{
    return (GetWorld()->GetTimeSeconds() - LastAttackTime) >= AttackInterval;
}

bool ACEnemyCharacterBase::IsInAttackDistance() const
{
    return Target && (DistToTarget() <= AttackEnterDistance);
}

bool ACEnemyCharacterBase::AcquireTarget()
{
    // 기존 타겟이 유효한지 확인
    if (Target && !Target->IsPendingKill() && Target->IsA<ACPlayerCharacter>())
        return true;

    // 모든 플레이어 캐릭터 찾기
    TArray<AActor*> Players;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACPlayerCharacter::StaticClass(), Players);
    
    if (Players.Num() == 0)
    {
        Target = nullptr;
        return false;
    }
    
    // 가장 가까운 플레이어 선택
    AActor* ClosestPlayer = nullptr;
    float MinDistance = FLT_MAX;
    
    for (AActor* Player : Players)
    {
        float Distance = FVector::Dist(GetActorLocation(), Player->GetActorLocation());
        if (Distance < MinDistance)
        {
            MinDistance = Distance;
            ClosestPlayer = Player;
        }
    }
    
    Target = ClosestPlayer;
    return (Target != nullptr);
}

bool ACEnemyCharacterBase::IsTargetInFOV(const AActor* Other) const
{
    if (!Other) return false;
    FVector Fwd = GetActorForwardVector(); Fwd.Z = 0.f; Fwd.Normalize();
    FVector To  = (Other->GetActorLocation() - GetActorLocation()); To.Z = 0.f;
    if (!To.Normalize()) return false;

    const float CosHalf = FMath::Cos(FMath::DegreesToRadians(SightFOVDegrees * 0.5f));
    const float Dot     = FVector::DotProduct(Fwd, To);
    return (Dot >= CosHalf);
}

bool ACEnemyCharacterBase::HasVisualOnTarget() const
{
    if (!Target) return false;

    // 1) 거리
    const float DistSq = FVector::DistSquared(GetActorLocation(), Target->GetActorLocation());
    if (DistSq > FMath::Square(SightDistance)) return false;

    // 2) FOV
    if (!IsTargetInFOV(Target)) return false;

    // 3) 라인트레이스(가림)
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(AI_LOS), false, this);
    const FVector S = GetActorLocation() + FVector(0,0, SightHeightOffsetSelf);
    const FVector E = Target->GetActorLocation() + FVector(0,0, SightHeightOffsetTarget);
    const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, S, E, SightTraceChannel, Params);
    return !bHit || Hit.GetActor() == Target;
}

float ACEnemyCharacterBase::DistToTarget() const
{
    return Target ? FVector::Dist(GetActorLocation(), Target->GetActorLocation()) : FLT_MAX;
}

void ACEnemyCharacterBase::RequestMoveTo(const FVector& Goal, float AcceptanceRadius)
{
    if (bUseNavigation)
    {
        if (AAIController* AI = Cast<AAIController>(GetController()))
        {
            FAIMoveRequest Req(Goal);
            Req.SetAcceptanceRadius(AcceptanceRadius);
            
            // 이동 요청 결과 처리
            FPathFollowingRequestResult Result = AI->MoveTo(Req);
            if (Result.Code == EPathFollowingRequestResult::Failed)
            {
                // 네비게이션 실패 시 직접 이동으로 전환
                bUseNavigation = false;
                DirectMoveGoal = Goal;
                DirectAcceptanceRadius = AcceptanceRadius;
                bDirectMoveActive = true;
            }
        }
    }
    else
    {
        DirectMoveGoal = Goal;
        DirectAcceptanceRadius = AcceptanceRadius;
        bDirectMoveActive = true;
    }
}

void ACEnemyCharacterBase::StopMove()
{
    if (bUseNavigation)
    {
        if (AAIController* AI = Cast<AAIController>(GetController()))
            AI->StopMovement();
    }
    else
    {
        bDirectMoveActive = false;
        if (UCharacterMovementComponent* M = GetCharacterMovement())
            M->StopMovementImmediately();
    }
}

bool ACEnemyCharacterBase::Reached(const FVector& P, float Radius) const
{
    return FVector::DistSquared(GetActorLocation(), P) <= FMath::Square(Radius);
}

void ACEnemyCharacterBase::DirectMoveTick(float /*DeltaSeconds*/)
{
    if (!bDirectMoveActive) return;

    if (Reached(DirectMoveGoal, DirectAcceptanceRadius))
    {
        bDirectMoveActive = false;
        return;
    }

    FVector Dir = (DirectMoveGoal - GetActorLocation()); Dir.Z = 0.f;
    if (Dir.Normalize())
        AddMovementInput(Dir, 1.f);
}

// ───────────────── 데미지/사망/드랍 ─────────────────

float ACEnemyCharacterBase::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
                                       AController* EventInstigator, AActor* DamageCauser)
{
    // 실제 HP 변화는 UCHealthComponent가 처리한다고 가정
    return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

void ACEnemyCharacterBase::OnHealthChanged(float Cur, float /*Max*/)
{
    if (Cur <= 0.f && State != EEnemyState::Dead)
    {
        SetState(EEnemyState::Dead);
        OnDead();
    }
}

void ACEnemyCharacterBase::OnDead()
{
    // 이동 및 충돌 비활성화
    GetCharacterMovement()->DisableMovement();
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    
    TryDropHealPack();
    
    // 애니메이션 재생 후 파괴 (3초 후)
    SetLifeSpan(3.0f);
}

void ACEnemyCharacterBase::TryDropHealPack()
{
    if (FMath::FRand() > HealPackDropChance) return;
    if (!HealPackClass) return;
    
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    
    FVector SpawnLocation = GetActorLocation() + FVector(0, 0, 50);
    FRotator SpawnRotation = FRotator::ZeroRotator;
    
    GetWorld()->SpawnActor<AActor>(HealPackClass, SpawnLocation, SpawnRotation, SpawnParams);
}

void ACEnemyCharacterBase::ApplyAttackDamage()
{
    if (!Target) return;
    
    // 공격 범위 내에 있는지 확인
    if (FVector::Dist(GetActorLocation(), Target->GetActorLocation()) <= AttackRange)
    {
        UGameplayStatics::ApplyDamage(Target, BaseDamage, GetController(), this, UDamageType::StaticClass());
    }
}

void ACEnemyCharacterBase::DebugDrawState()
{
#if ENABLE_DRAW_DEBUG
    // 상태 정보 표시
    FString StateName = UEnum::GetValueAsString(State);
    FString DebugText = FString::Printf(TEXT("State: %s"), *StateName);
    GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Yellow, DebugText);
    
    // 시야 범위 시각화
    FVector Start = GetActorLocation() + FVector(0, 0, SightHeightOffsetSelf);
    float AngleRad = FMath::DegreesToRadians(SightFOVDegrees);
    FVector LeftDir = GetActorForwardVector().RotateAngleAxis(SightFOVDegrees * 0.5f, FVector::UpVector);
    FVector RightDir = GetActorForwardVector().RotateAngleAxis(-SightFOVDegrees * 0.5f, FVector::UpVector);
    
    DrawDebugLine(GetWorld(), Start, Start + LeftDir * SightDistance, FColor::Green, false, 0.0f, 0, 1.0f);
    DrawDebugLine(GetWorld(), Start, Start + RightDir * SightDistance, FColor::Green, false, 0.0f, 0, 1.0f);
    
    // 타겟과의 연결선
    if (Target)
    {
        DrawDebugLine(GetWorld(), Start, Target->GetActorLocation(), 
                      HasVisualOnTarget() ? FColor::Red : FColor::Blue, false, 0.0f, 0, 1.0f);
    }
#endif
}

