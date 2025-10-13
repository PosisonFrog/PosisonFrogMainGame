// Source/PosisonFrog/00_Character/01_Enemy/Components/CTankerChargeComponent.cpp
#include "CTankerChargeComponent.h"

#include "AIController.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "99_Util/CLog.h"

UCTankerChargeComponent::UCTankerChargeComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    DamageTypeClass = UDamageType::StaticClass(); // Fury/스택 비연동(항상 일반 데미지)
}

void UCTankerChargeComponent::BeginPlay()
{
    Super::BeginPlay();

    OwnerChar = Cast<ACharacter>(GetOwner());
    if (OwnerChar.IsValid())
    {
        MoveComp = OwnerChar->GetCharacterMovement();
        AI = Cast<AAIController>(OwnerChar->GetController());

        OwnerChar->OnTakeAnyDamage.AddDynamic(this, &UCTankerChargeComponent::HandleOwnerDamaged);
    }
}

void UCTankerChargeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (AI.IsValid())
    {
        AI->StopMovement();
    }
    
    ClearAllTimers();
    ResetChargeState();
    State = EChargeState::Idle;

    if (OwnerChar.IsValid())
    {
        OwnerChar->OnTakeAnyDamage.RemoveDynamic(this, &UCTankerChargeComponent::HandleOwnerDamaged);
    }
    
    OwnerChar.Reset();
    MoveComp.Reset();
    AI.Reset();
    
    Super::EndPlay(EndPlayReason);
}

void UCTankerChargeComponent::TickComponent(float DeltaTime, ELevelTick, FActorComponentTickFunction*)
{
    if (!OwnerChar.IsValid() || !MoveComp.IsValid()) return;

    if (State == EChargeState::PreCharge)
    {
        PreChargeTick(DeltaTime);
        return;
    }

    if (State == EChargeState::Charging)
    {
        if (IsValidTarget())
        {
            const FVector To = (TargetActor->GetActorLocation() - OwnerChar->GetActorLocation());
            FaceTowards(To, DeltaTime);
            ChargeDir2D = To.GetSafeNormal2D();
        }

        FVector Vel = ChargeDir2D * ChargeSpeed;
        Vel.Z = MoveComp->Velocity.Z;
        MoveComp->Velocity = Vel;

        ChargeTraceAndHit(DeltaTime);
    }
}

bool UCTankerChargeComponent::RequestCharge(AActor* InTarget)
{
    if (!OwnerChar.IsValid() || !MoveComp.IsValid()) return false;
    if (State != EChargeState::Idle) return false;

    UWorld* World = GetWorld();
    if (!World) return false;
    
    TargetActor = InTarget;
    if (!IsValidTarget()) return false;

    const float Dist = DistToTarget2D();
    if (Dist > ChargeMaxDistance) return false;

    const bool bTooCloseForPreCharge = (Dist < ChargeMinDistance);
    
    bool bHasPreChargeGoal = false;
    FVector CandidateGoal = FVector::ZeroVector;
    int32  CandidateSide = LastSideSign;
    
    if (bUsePreChargeOffset && !bTooCloseForPreCharge)
    {
        int32  SideSign = LastSideSign;
        FVector Goal;
        if (ComputePreChargeGoal(Goal, SideSign))
        {
            bHasPreChargeGoal = true;
            CandidateGoal = Goal;
            CandidateSide = SideSign;
            
        }
    }
    
    
    if (!bHasPreChargeGoal && !bTooCloseForPreCharge && Dist < ChargeMinDistance)
        return false;

    if (AI.IsValid()) AI->StopMovement();

    // 1) PreCharge(사선 오프셋) 우선
    //if (bUsePreChargeOffset && ComputePreChargeGoal(PreChargeGoal, LastSideSign))
    if (bHasPreChargeGoal)
    {
        PreChargeGoal = CandidateGoal;
        LastSideSign  = CandidateSide;
        EnterState(EChargeState::PreCharge);
        PreChargeStartTime = GetWorld()->GetTimeSeconds();
        bPreChargeUsingNav = AI.IsValid();

        if (bPreChargeUsingNav)
            AI->MoveToLocation(PreChargeGoal, PreChargeAcceptanceRadius, /*bStopOnOverlap*/false);

        // 타임아웃 폴백: 멈추지 않고 진행
        FTimerManager& TM = World->GetTimerManager();
        TM.ClearTimer(TH_PreChargeTimeout);
        TM.SetTimer(TH_PreChargeTimeout, this, &UCTankerChargeComponent::BeginWindup, PreChargeMaxTime, false);
        return true;
    }

    // 2) 바로 Windup (근접한 상황에서는 바로 돌진 준비)
    BeginWindup();
    return true;
}

void UCTankerChargeComponent::AbortCharge()
{
    switch (State)
    {
    case EChargeState::PreCharge:
    case EChargeState::Windup:
    case EChargeState::Charging:
        EndCharging(EChargeEndReason::Aborted, nullptr);
        break;
    case EChargeState::Recovery:
        BeginCooldown();
        break;
    default:
        break;
    }
}

void UCTankerChargeComponent::Anim_ChargeStart()
{
    if (State == EChargeState::Windup)
        BeginCharging();
}

bool UCTankerChargeComponent::ComputePreChargeGoal(FVector& OutGoal, int32& OutSideSign)
{
    if (!OwnerChar.IsValid() || !IsValidTarget()) return false;

    const FVector OwnerLoc  = OwnerChar->GetActorLocation();
    const FVector TargetLoc = TargetActor->GetActorLocation();

    // 타겟 → 오너 벡터(원형 배치 기준 반지름 방향)
    FVector FromTarget = (OwnerLoc - TargetLoc); FromTarget.Z = 0.f;
    if (!FromTarget.Normalize())
        FromTarget = -TargetActor->GetActorForwardVector().GetSafeNormal2D();

    const FVector Right = FVector(-FromTarget.Y, FromTarget.X, 0.f);

    // 좌/우 번갈이(혼잡 분산)
    OutSideSign = (bAlternateSideBetweenCharges ? -OutSideSign : (FMath::RandBool() ? +1 : -1));

    FVector Candidate = TargetLoc + FromTarget * PreChargeDistance + Right * (OutSideSign * PreChargeLateralOffset);

    UWorld* World = GetWorld();
    
    if (bClampToNavMesh && World)
    {
        if (UNavigationSystemV1* NS = UNavigationSystemV1::GetCurrent(World))
        {
            FNavLocation Projected;
            if (NS->ProjectPointToNavigation(Candidate, Projected, FVector(200,200,200)))
            {
                OutGoal = Projected.Location;
                if (bDrawPreChargeGoal) DrawDebugSphere(World, OutGoal, 30.f, 12, FColor::Green, false, 1.0f);
                return true;
            }
        }
    }

    OutGoal = Candidate;
    if (bDrawPreChargeGoal) DrawDebugSphere(GetWorld(), OutGoal, 30.f, 12, FColor::Yellow, false, 1.0f);
    return true;
}

void UCTankerChargeComponent::PreChargeTick(float DeltaSeconds)
{
    if (State != EChargeState::PreCharge) return;

    if (!OwnerChar.IsValid() || !MoveComp.IsValid())
    {
        ClearAllTimers();
        ResetChargeState();
        State = EChargeState::Idle;
        return;
    }
    
    UWorld* World = GetWorld();
    if (!World)
        return;
    
    // Nav 사용 시: 도달 판정만
    if (bPreChargeUsingNav)
    {
        if (Reached(PreChargeGoal, PreChargeAcceptanceRadius))
        {
            if (AI.IsValid()) AI->StopMovement();
            World->GetTimerManager().ClearTimer(TH_PreChargeTimeout);
            BeginWindup();
        }
        return;
    }

    // 비Nav: 직진 스티어링
    FVector To = (PreChargeGoal - OwnerChar->GetActorLocation()); To.Z = 0.f;
    const float Dist = To.Size();
    if (Dist <= PreChargeAcceptanceRadius)
    {
        World->GetTimerManager().ClearTimer(TH_PreChargeTimeout);
        BeginWindup();
        return;
    }

    if (To.Normalize())
    {
        FaceTowards(To, DeltaSeconds);
        MoveComp->MaxWalkSpeed = FMath::Max(MoveComp->MaxWalkSpeed, PreChargeMoveSpeed);
        OwnerChar->AddMovementInput(To, 1.f);
    }
}

void UCTankerChargeComponent::BeginWindup()
{
    if (!OwnerChar.IsValid())
    {
        ClearAllTimers();
        ResetChargeState();
        State = EChargeState::Idle;
        return;
    }
 

    if (AI.IsValid())
    {
        AI->StopMovement();
    }
    
    if (UWorld* World = GetWorld())
    {
        FTimerManager& TM = World->GetTimerManager();
        TM.ClearTimer(TH_PreChargeTimeout);
        
        EnterState(EChargeState::Windup);
        HitActorsThisCharge.Reset();
        
        // 애님 노티가 없더라도 폴백으로 진행
        const float WindupDuration = bStartOnAnimNotify ? WindupTime + 0.1f : WindupTime;
        TM.SetTimer(TH_Windup, this, &UCTankerChargeComponent::BeginCharging, WindupDuration, false);
    }
}

void UCTankerChargeComponent::BeginCharging()
{
    if (State != EChargeState::Windup) return;

    if (!OwnerChar.IsValid() || !MoveComp.IsValid())
    {
        ClearAllTimers();
        ResetChargeState();
        State = EChargeState::Idle;
        return;
    }

    if (UWorld* World = GetWorld())
    {
        FTimerManager& TM = World->GetTimerManager();
        TM.ClearTimer(TH_Windup);
        
        if (!IsValidTarget())
        {
            BeginCooldown();
            return;
        }
        
        EnterState(EChargeState::Charging);
        ChargeStartTime = World->GetTimeSeconds();
        ChargeDir2D = (TargetActor->GetActorLocation() - OwnerChar->GetActorLocation()).GetSafeNormal2D();
        
        TM.SetTimer(TH_MaxCharge, this, &UCTankerChargeComponent::HandleMaxChargeTimeElapsed, MaxChargeTime, false);
    }
}

void UCTankerChargeComponent::EndCharging(EChargeEndReason Reason, AActor* HitActor)
{
    if (State != EChargeState::Charging && State != EChargeState::Windup && State != EChargeState::PreCharge)
        return;

    GetWorld()->GetTimerManager().ClearTimer(TH_MaxCharge);
    GetWorld()->GetTimerManager().ClearTimer(TH_PreChargeTimeout);

    if (MoveComp.IsValid()) MoveComp->StopMovementImmediately();

    BeginRecovery(Reason, HitActor);
}

void UCTankerChargeComponent::BeginRecovery(EChargeEndReason Reason, AActor* HitActor)
{
    if (!OwnerChar.IsValid())
    {
        ClearAllTimers();
        ResetChargeState();
        State = EChargeState::Idle;
        return;
    }
    
    EnterState(EChargeState::Recovery);
    OnChargeFinished.Broadcast(Reason, HitActor);
 

    if (UWorld* World = GetWorld())
    {
        const bool bFailedCharge = (Reason == EChargeEndReason::Aborted)
            || (Reason == EChargeEndReason::MaxTime)
            || (Reason == EChargeEndReason::MaxDistance);
        
        const float Stun = (Reason == EChargeEndReason::HitWorld)
        ? WallStunTime
        : (bFailedCharge ? FailedChargeStunTime : RecoveryTime);

        bPendingFailedChargeRecovery = false;
        
        if (bFailedCharge)
        {
            bPendingFailedChargeRecovery = true;
            const float RecoveryDelay = (FailedChargeRecoveryDelay > 0.f)
                ? FMath::Min(Stun, FailedChargeRecoveryDelay)
                : 0.f;
                
            if (RecoveryDelay <= KINDA_SMALL_NUMBER)
            {
                FinishFailedChargeRecovery();
            }
            else
            {
                World->GetTimerManager().SetTimer(TH_Recovery, this,
                &UCTankerChargeComponent::HandleFailedChargeRecoveryTimeout,
                RecoveryDelay, false);
            }
        }
        else
        {
            World->GetTimerManager().SetTimer(TH_Recovery, this, &UCTankerChargeComponent::BeginCooldown, Stun, false);
        }
    }
}

void UCTankerChargeComponent::BeginCooldown()
{
    if (!OwnerChar.IsValid())
    {
        ClearAllTimers();
        ResetChargeState();
        State = EChargeState::Idle;
        return;
    }

    if (State != EChargeState::Recovery)
        return;

    bPendingFailedChargeRecovery = false;
    
    EnterState(EChargeState::Cooldown);
 
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(TH_Cooldown, this, &UCTankerChargeComponent::HandleCooldownFinished, ChargeCooldown, false);
    }
}

void UCTankerChargeComponent::HandleOwnerDamaged(AActor* DamagedActor, float Damage, const UDamageType* /*DamageType*/,
    AController* /*InstigatedBy*/, AActor* /*DamageCauser*/)
{
    if (Damage <= 0.f)
        return;
    
    if (DamagedActor != OwnerChar.Get())
        return;
    
    if (!bPendingFailedChargeRecovery)
        return;
    
    if (State != EChargeState::Recovery)
        return;
    
        FinishFailedChargeRecovery();
}

void UCTankerChargeComponent::HandleFailedChargeRecoveryTimeout()
{
    FinishFailedChargeRecovery();
}

void UCTankerChargeComponent::FinishFailedChargeRecovery()
{
    if (!bPendingFailedChargeRecovery)
        return;

    bPendingFailedChargeRecovery = false;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(TH_Recovery);
    }
    
    BeginCooldown();
}


void UCTankerChargeComponent::ChargeTraceAndHit(float /*DeltaSeconds*/)
{
    if (!OwnerChar.IsValid())
        return;
    
    UWorld* World = GetWorld();
    if (!World)
        return;
    
    FHitResult Hit;
    if (!SweepHitAhead(Hit, TraceAhead)) return;
 
    AActor* Other = Hit.GetActor();
    if (!Other) return;
 
    // 월드 충돌
    if (Hit.Component.IsValid() && Hit.Component->GetCollisionObjectType() == ECC_WorldStatic)
    {
        EndCharging(EChargeEndReason::HitWorld, Other);
        return;
    }
 
    // Pawn 충돌 (팀/타입 필터 적용)
    if (!HitActorsThisCharge.Contains(Other))
    {
        // 자기 자신 제외
        if (Other == OwnerChar.Get()) return;
 
        // Pawn만 타격
        APawn* HitPawn = Cast<APawn>(Other);
        if (!HitPawn) return;
 
        // 플레이어만 타격 (적 → 플레이어 돌진)
        if (!HitPawn->IsPlayerControlled())

            HitActorsThisCharge.Add(Other);
        AController* Inst = OwnerChar.IsValid() ? OwnerChar->GetController() : nullptr;
        UGameplayStatics::ApplyDamage(Other, HitDamage, Inst, OwnerChar.Get(), DamageTypeClass);
         
        EndCharging(EChargeEndReason::HitPawn, Other);
    }
 
    /* // Pawn 충돌 (필요 시 팀/태그 필터) - DamageTypeClass ? DamageTypeClass가 모호하며, AppluDamage 인수개수 에러
    if (!HitActorsThisCharge.Contains(Other))
    {
        HitActorsThisCharge.Add(Other);

        AController* Inst = OwnerChar.IsValid() ? OwnerChar->GetController() : nullptr;
        UGameplayStatics::ApplyDamage(
            Other, HitDamage, Inst, OwnerChar.Get(),
            DamageTypeClass ? DamageTypeClass : UDamageType::StaticClass());

        EndCharging(EChargeEndReason::HitPawn, Other);
    }*/
}

bool UCTankerChargeComponent::SweepHitAhead(FHitResult& OutHit, float Distance) const
{
    if (!OwnerChar.IsValid()) return false;

    UWorld* World = GetWorld();
    if (!World) return false;
    
    const FVector Start = OwnerChar->GetActorLocation();
    const FVector Dir   = ChargeDir2D.IsNearlyZero()
                        ? OwnerChar->GetActorForwardVector().GetSafeNormal2D()
                        : ChargeDir2D;
    const FVector End   = Start + Dir * Distance;

    FCollisionQueryParams P(SCENE_QUERY_STAT(TankerCharge), false, OwnerChar.Get());
    FCollisionShape Capsule = FCollisionShape::MakeCapsule(TraceRadius, OwnerChar->GetSimpleCollisionHalfHeight());

    // Pawn → WorldStatic 순서로 검사
    if (World->SweepSingleByChannel(OutHit, Start, End, FQuat::Identity, ECC_Pawn, Capsule, P))
        return true;

    return World->SweepSingleByChannel(OutHit, Start, End, FQuat::Identity, ECC_WorldStatic, Capsule, P);
}

bool UCTankerChargeComponent::IsValidTarget() const
{
    return TargetActor.IsValid() && !TargetActor->IsPendingKill();
}

float UCTankerChargeComponent::DistToTarget2D() const
{
    if (!OwnerChar.IsValid() || !TargetActor.IsValid()) return TNumericLimits<float>::Max();
    return FVector::Dist2D(OwnerChar->GetActorLocation(), TargetActor->GetActorLocation());
}

void UCTankerChargeComponent::FaceTowards(const FVector& To, float DeltaSeconds)
{
    if (!OwnerChar.IsValid()) return;
    FRotator R = OwnerChar->GetActorRotation();
    const FRotator TargetYaw(0.f, To.Rotation().Yaw, 0.f);
    R.Yaw = FMath::FixedTurn(R.Yaw, TargetYaw.Yaw, TurnRateDegPerSec * DeltaSeconds);
    OwnerChar->SetActorRotation(R);
}

void UCTankerChargeComponent::EnterState(EChargeState NewState)
{
    if (State == NewState) return;
    EChargeState Prev = State;
    State = NewState;
    OnChargeStateChanged.Broadcast(State, Prev);

#if !(UE_BUILD_SHIPPING)
    if (bDrawPreChargeGoal && NewState == EChargeState::PreCharge)
    {
        DrawDebugSphere(GetWorld(), PreChargeGoal, 30.f, 12, FColor::Cyan, false, 2.0f);
        if (TargetActor.IsValid())
        {
            DrawDebugCircle(GetWorld(), TargetActor->GetActorLocation(),
                PreChargeDistance, 32, FColor::Blue, false, 2.0f, 0, 1.0f,
                FVector(1,0,0), FVector(0,1,0), false);
        }
    }
#endif
}

void UCTankerChargeComponent::ExitState(EChargeState /*OldState*/) {}

bool UCTankerChargeComponent::Reached(const FVector& P, float Radius) const
{
    if (!OwnerChar.IsValid()) return false;
    return FVector::DistSquared(OwnerChar->GetActorLocation(), P) <= FMath::Square(Radius);
}

void UCTankerChargeComponent::ClearAllTimers()
{
    if (UWorld* World = GetWorld())
    {
        FTimerManager& TM = World->GetTimerManager();
        TM.ClearTimer(TH_Windup);
        TM.ClearTimer(TH_MaxCharge);
        TM.ClearTimer(TH_Recovery);
        TM.ClearTimer(TH_Cooldown);
        TM.ClearTimer(TH_PreChargeTimeout);
    }

    bPendingFailedChargeRecovery = false;
}

void UCTankerChargeComponent::ResetChargeState()
{
    HitActorsThisCharge.Reset();
    TargetActor = nullptr;
    bPreChargeUsingNav = false;
    ChargeDir2D = FVector::ForwardVector;
}

void UCTankerChargeComponent::HandleMaxChargeTimeElapsed()
{
    EndCharging(EChargeEndReason::MaxTime, nullptr);
}

void UCTankerChargeComponent::HandleCooldownFinished()
{
    ClearAllTimers();
    ResetChargeState();
    EnterState(EChargeState::Idle);
}


