#include "CTankerChargeComponent.h"

#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "Components/CapsuleComponent.h"


UCTankerChargeComponent::UCTankerChargeComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    if (!DamageTypeClass)
    {
        DamageTypeClass = UDamageType::StaticClass();
    }
}

void UCTankerChargeComponent::BeginPlay()
{
    Super::BeginPlay();
    EnsureOwnerAndMovement();
}

void UCTankerChargeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (CachedAI.IsValid())
    {
        CachedAI->StopMovement();
    }
    
    ClearTimers();
    ResetTransientData();
    State = EChargeState::Idle;

    /*
    if (OwnerChar.IsValid())
    {
        OwnerChar->OnTakeAnyDamage.RemoveDynamic(this, &UCTankerChargeComponent::HandleOwnerDamaged);
    }*/
    
    OwnerChar.Reset();
    MoveComp.Reset();
    TargetActor.Reset();
    CachedAI.Reset();
    
    Super::EndPlay(EndPlayReason);
}

void UCTankerChargeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    if (!EnsureOwnerAndMovement())
    {
        return;
    }
    switch (State)
    {
    case EChargeState::PreCharge:
        TickPreCharge(DeltaTime);
        break;
    case EChargeState::Charging:
        UpdateCharging(DeltaTime);
        break;
    default:
        break;
    }
}

bool UCTankerChargeComponent::RequestCharge(AActor* InTarget)
{
    if (!EnsureOwnerAndMovement())
    {
        return false;
    }
    if (State != EChargeState::Idle)
    {
        return false;
    }
    
    UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }
    
    TargetActor = InTarget;
    if (!HasValidTarget())
    {
        return false;
    }

    const float Distance2D = DistanceToTarget2D();
    if (Distance2D > ChargeMaxDistance)
    {
        return false;
    }

    const bool bTooCloseForPreCharge = Distance2D <= ChargeMaxDistance;

    bool bHasPreChargeGoal = false;
    FVector Goal = FVector::ZeroVector;
    int32 SideSign = LastSideSign;
    
    if (bUsePreChargeOffset && !bTooCloseForPreCharge)
    {
        bHasPreChargeGoal = TryBuildPreChargeGoal(Goal, SideSign);
    }
    

    if (CachedAI.IsValid())
    {
        CachedAI->StopMovement();
    }
    
    // 1) PreCharge(사선 오프셋) 
    if (bHasPreChargeGoal)
    {
        PreChargeGoal = Goal;
        LastSideSign = SideSign;
        PreChargeStartTime = World->GetTimeSeconds();
        bPreChargeUsingNav = CachedAI.IsValid();

        EnterState(EChargeState::PreCharge);

        if (bPreChargeUsingNav)
        {
            CachedAI->MoveToLocation(PreChargeGoal, PreChargeAcceptanceRadius, /*bStopOnOverlap*/false);
        }

        FTimerManager& TimerManager = World->GetTimerManager();
        TimerManager.ClearTimer(TH_PreChargeTimeout);
        TimerManager.SetTimer(TH_PreChargeTimeout, this, &UCTankerChargeComponent::BeginWindupInternal, PreChargeMaxTime, false);
        return true;
    }

    // 2) 바로 Windup (근접한 상황에서는 바로 돌진 준비)
    BeginWindupInternal();
    return true;
}

void UCTankerChargeComponent::AbortCharge()
{
    switch (State)
    {
    case EChargeState::PreCharge:
    case EChargeState::Windup:
    case EChargeState::Charging:
        EndChargingInternal(EChargeEndReason::Aborted, nullptr);
        break;
    case EChargeState::Recovery:
        StartCooldownInternal();
        break;
    default:
        break;
    }
}

bool UCTankerChargeComponent::IsChargingOrWindup() const
{
    return State == EChargeState::PreCharge
        || State == EChargeState::Windup
        || State == EChargeState::Charging;
}


void UCTankerChargeComponent::ResetForRespawn()
{
    ClearTimers();
    ResetTransientData();
    
    PreChargeGoal = FVector::ZeroVector;
    PreChargeStartTime = 0.f;
    ChargeStartTime = 0.f;
    LastSideSign = +1;
    
    if (CachedAI.IsValid())
    {
        CachedAI->StopMovement();
    }
    
    if (OwnerChar.IsValid())
    {
        if (UCharacterMovementComponent* Movement = OwnerChar->GetCharacterMovement())
        {
            Movement->StopMovementImmediately();
            Movement->Velocity = FVector::ZeroVector;
        }
            
        MoveComp = OwnerChar->GetCharacterMovement();
        CachedAI = Cast<AAIController>(OwnerChar->GetController());
    }
    else
    {
        EnsureOwnerAndMovement();
    }
    
    const EChargeState PreviousState = State;
    State = EChargeState::Idle;
    if (PreviousState != State)
    {
        OnChargeStateChanged.Broadcast(State, PreviousState);
    }
    UE_LOG(LogTemp, Log, TEXT("[ChargeComp] 리스폰 리셋 완료: State -> Idle"));
}

void UCTankerChargeComponent::Anim_ChargeStart()
{
    if (State == EChargeState::Windup)
        BeginChargingInternal();
}

void UCTankerChargeComponent::EnterState(EChargeState NewState)
{
    if (State == NewState)
    {
        return;
    }

    const EChargeState Previous = State;
    State = NewState;
    OnChargeStateChanged.Broadcast(State, Previous);

    UWorld* World = GetWorld();
    
#if !(UE_BUILD_SHIPPING)
    if (bDrawPreChargeGoal && NewState == EChargeState::PreCharge)
    {
        if (bDrawPreChargeGoal && NewState == EChargeState::PreCharge)
        {
            DrawDebugSphere(World, PreChargeGoal, 30.f, 12, FColor::Cyan, false, 2.f);
            if (TargetActor.IsValid())
            {
                DrawDebugCircle(World, TargetActor->GetActorLocation(), PreChargeDistance, 32, FColor::Blue, false, 2.f, 0, 1.f, FVector::RightVector, FVector::ForwardVector, false);
            }
        }
    }
#endif
}

void UCTankerChargeComponent::ResetTransientData()
{
    HitActorsThisCharge.Reset();
    TargetActor.Reset();
    bPreChargeUsingNav = false;
    ChargeDirection = FVector::ForwardVector;
    bPendingFailedChargeRecovery = false;
}

void UCTankerChargeComponent::ClearTimers()
{
    if (UWorld* World = GetWorld())
    {
        FTimerManager& TimerManager = World->GetTimerManager();
        TimerManager.ClearTimer(TH_Windup);
        TimerManager.ClearTimer(TH_MaxCharge);
        TimerManager.ClearTimer(TH_Recovery);
        TimerManager.ClearTimer(TH_Cooldown);
        TimerManager.ClearTimer(TH_PreChargeTimeout);
    }
    bPendingFailedChargeRecovery = false;
}

void UCTankerChargeComponent::BeginWindupInternal()
{
    if (!EnsureOwnerAndMovement())
    {
        ClearTimers();
        ResetTransientData();
        State = EChargeState::Idle;
        return;
    }
 

    if (CachedAI.IsValid())
    {
        CachedAI->StopMovement();
    }
    
    if (UWorld* World = GetWorld())
    {
        FTimerManager& TimerManager = World->GetTimerManager();
        TimerManager.ClearTimer(TH_PreChargeTimeout);
        
        EnterState(EChargeState::Windup);
        HitActorsThisCharge.Reset();
        
        // 애님 노티가 없더라도 폴백으로 진행
        const float WindupDuration = bStartOnAnimNotify ? WindupTime + 0.1f : WindupTime;
        TimerManager.SetTimer(TH_Windup, this, &UCTankerChargeComponent::BeginChargingInternal, WindupDuration, false);
    }
}

void UCTankerChargeComponent::BeginChargingInternal()
{
    if (State != EChargeState::Windup)
    {
        return;
    }
    
    if (!EnsureOwnerAndMovement())
    {
        ClearTimers();
        ResetTransientData();
        State = EChargeState::Idle;
        return;
    }

    if (!HasValidTarget())
    {
        StartCooldownInternal();
        return;
    }
    
    if (UWorld* World = GetWorld())
    {
        FTimerManager& TimerManager = World->GetTimerManager();
        TimerManager.ClearTimer(TH_Windup);
        
        EnterState(EChargeState::Charging);
        ChargeStartTime = World->GetTimeSeconds();
        ChargeDirection  = (TargetActor->GetActorLocation() - OwnerChar->GetActorLocation()).GetSafeNormal2D();

        if (UCapsuleComponent* Capsule = OwnerChar->FindComponentByClass<UCapsuleComponent>())
        {
            Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
        }
        
        TimerManager.SetTimer(TH_MaxCharge, this, &UCTankerChargeComponent::HandleMaxChargeTime, MaxChargeTime, false);
    }
}

void UCTankerChargeComponent::EndChargingInternal(EChargeEndReason Reason, AActor* HitActor)
{
    if (State != EChargeState::Charging && State != EChargeState::Windup && State != EChargeState::PreCharge)
    {
        return;
    }
 
    if (UWorld* World = GetWorld())
    {
        FTimerManager& TimerManager = World->GetTimerManager();
        TimerManager.ClearTimer(TH_MaxCharge);
        TimerManager.ClearTimer(TH_PreChargeTimeout);

        if (UCapsuleComponent* Capsule = OwnerChar->FindComponentByClass<UCapsuleComponent>())
        {
            Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
        }
    }
 
    if (MoveComp.IsValid())
    {
        MoveComp->StopMovementImmediately();
    }
    
    BeginRecoveryInternal(Reason, HitActor);
}

void UCTankerChargeComponent::BeginRecoveryInternal(EChargeEndReason Reason, AActor* HitActor)
{
    if (!OwnerChar.IsValid())
    {
        ClearTimers();
        ResetTransientData();
        State = EChargeState::Idle;
        return;
    }
    
    EnterState(EChargeState::Recovery);
    OnChargeFinished.Broadcast(Reason, HitActor);

    const bool bFailedCharge = (Reason == EChargeEndReason::Aborted)
          || (Reason == EChargeEndReason::MaxTime)
          || (Reason == EChargeEndReason::MaxDistance);
   
    if (bFailedCharge)
    {
        StartCooldownInternal();
        return;
    } 

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(TH_Recovery);
        
        const float StunDuration = (Reason == EChargeEndReason::HitWorld) ? WallStunTime : RecoveryTime;
        World->GetTimerManager().SetTimer(TH_Recovery, this, &UCTankerChargeComponent::StartCooldownInternal, StunDuration, false);
    }
}

void UCTankerChargeComponent::StartCooldownInternal()
{
    if (!EnsureOwnerAndMovement())
    {
        ClearTimers();
        ResetTransientData();
        State = EChargeState::Idle;
        return;
    }

    if (State != EChargeState::Recovery)
    {
        EnterState(EChargeState::Recovery);
    }
    
    EnterState(EChargeState::Cooldown);
 
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(TH_Recovery);
        World->GetTimerManager().SetTimer(TH_Cooldown, this, &UCTankerChargeComponent::HandleCooldownFinished, ChargeCooldown, false);
        FTimerManager& TM = World->GetTimerManager();
    }
}

void UCTankerChargeComponent::HandleMaxChargeTime()
{
    EndChargingInternal(EChargeEndReason::MaxTime, nullptr);
}

void UCTankerChargeComponent::HandleCooldownFinished()
{
    ClearTimers();
    ResetTransientData();
    EnterState(EChargeState::Idle);
}

/*void UCTankerChargeComponent::HandleFailedChargeRecoveryTimeout()
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
*/


bool UCTankerChargeComponent::TryBuildPreChargeGoal(FVector& OutGoal, int32& OutSideSign)
{
    if (!EnsureOwnerAndMovement() || !HasValidTarget())
    {
        return false;
    }
    const FVector OwnerLocation = OwnerChar->GetActorLocation();
    const FVector TargetLocation = TargetActor->GetActorLocation();
    FVector FromTarget = OwnerLocation - TargetLocation;
    FromTarget.Z = 0.f;

    if (!FromTarget.Normalize())
    {
        FromTarget = -TargetActor->GetActorForwardVector().GetSafeNormal2D();
    }

    const FVector RightVector(-FromTarget.Y, FromTarget.X, 0.f);
    
    if (bAlternateSideBetweenCharges)
    {
        OutSideSign = -OutSideSign;
    }
    else
    {
        OutSideSign = FMath::RandBool() ? +1 : -1;
    }
    
    FVector Candidate = TargetLocation + FromTarget * PreChargeDistance + RightVector * (OutSideSign * PreChargeLateralOffset);
    
    UWorld* World = GetWorld();
    if (!World)
    {
        OutGoal = Candidate;
        return true;
    }
    
    if (bClampToNavMesh)
    {
        if (UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World))
        {
            FNavLocation Projected;
            if (NavSys->ProjectPointToNavigation(Candidate, Projected, FVector(200.f)))
            {
                OutGoal = Projected.Location;
                if (bDrawPreChargeGoal)
                {
                    DrawDebugSphere(World, OutGoal, 30.f, 12, FColor::Green, false, 1.f);
                }
                return true;
            }
        }
    }
    
    OutGoal = Candidate;
    if (bDrawPreChargeGoal)
    {
        DrawDebugSphere(World, OutGoal, 30.f, 12, FColor::Yellow, false, 1.f);
    }
    return true;
}

void UCTankerChargeComponent::TickPreCharge(float DeltaSeconds)
{
    if (State != EChargeState::PreCharge)
    {
        return;
    }
    
    if (!EnsureOwnerAndMovement()){
        ClearTimers();
        ResetTransientData();
        State = EChargeState::Idle;
        return;
    }
    
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    if (bPreChargeUsingNav)
    {
        if (HasReached(PreChargeGoal, PreChargeAcceptanceRadius))
        {
            if (CachedAI.IsValid())
            {
                CachedAI->StopMovement();
                World->GetTimerManager().ClearTimer(TH_PreChargeTimeout);
                BeginWindupInternal();
            }
            return;
        }
        FVector Direction = PreChargeGoal - OwnerChar->GetActorLocation();
        Direction.Z = 0.f;
        if (Direction.Size() <= PreChargeAcceptanceRadius)
        {
            World->GetTimerManager().ClearTimer(TH_PreChargeTimeout);
            BeginWindupInternal();
            return;
        }
 
       
        if (Direction.Normalize())
        {
            FaceTowards(Direction, DeltaSeconds);
            MoveComp->MaxWalkSpeed = FMath::Max(MoveComp->MaxWalkSpeed, PreChargeMoveSpeed);
            OwnerChar->AddMovementInput(Direction, 1.f);
        }
    }
}

bool UCTankerChargeComponent::HasReached(const FVector& Point, float Radius) const
{
    if (!OwnerChar.IsValid())
    {
        return false;
    }
    return FVector::DistSquared(OwnerChar->GetActorLocation(), Point) <= FMath::Square(Radius);
}

void UCTankerChargeComponent::UpdateCharging(float DeltaSeconds)
{
    if (!EnsureOwnerAndMovement())
    {
        EndChargingInternal(EChargeEndReason::Aborted, nullptr);
        return;
    }
   
    if (HasValidTarget())
    {
        const FVector ToTarget = TargetActor->GetActorLocation() - OwnerChar->GetActorLocation();
        FaceTowards(ToTarget, DeltaSeconds);
        ChargeDirection = ToTarget.GetSafeNormal2D();

        const float HorizontalDistance = FVector::Dist2D(TargetActor->GetActorLocation(), OwnerChar->GetActorLocation());
        const float VerticalDistance = FMath::Abs(TargetActor->GetActorLocation().Z - OwnerChar->GetActorLocation().Z);
        if (HorizontalDistance <= ChargeVerticalAbortHorizontalTolerance
            && VerticalDistance >= ChargeVerticalAbortHeight)
        {
            EndChargingInternal(EChargeEndReason::MaxDistance, TargetActor.Get());
            return;
        }
    }
  
    FVector Velocity = ChargeDirection * ChargeSpeed;
    Velocity.Z = MoveComp->Velocity.Z;
    MoveComp->Velocity = Velocity;
   
    PerformChargeTrace();
}

void UCTankerChargeComponent::PerformChargeTrace()
{
    if (!OwnerChar.IsValid())
    {
        return;
    }
    FHitResult Hit;
    if (!SweepAhead(Hit, TraceAhead))
    {
        return;
    }
    
    AActor* Other = Hit.GetActor();
    
    if (!Other)
    {
        return;
    }

    if (Hit.Component.IsValid() && Hit.Component->GetCollisionObjectType() == ECC_WorldStatic)
    {
        EndChargingInternal(EChargeEndReason::HitWorld, Other);
        return;
    }
  
    if (HitActorsThisCharge.Contains(Other))
    {
        return;
    }
    if (Other == OwnerChar.Get())
    {
        return;
    }
   
    APawn* HitPawn = Cast<APawn>(Other);
    if (!HitPawn)
    {
        return;
    }

    AController* HitController = HitPawn->GetController();
    AAIController* AIController = Cast<AAIController>(HitController);
    if (AIController)
    {
        return;  
    }
   
    HitActorsThisCharge.Add(Other);

    if (HitPawn->IsPlayerControlled())
    {
        AController* InstigatorController = OwnerChar.IsValid() ? OwnerChar->GetController() : nullptr;
        UGameplayStatics::ApplyDamage(Other, HitDamage, InstigatorController, OwnerChar.Get(), DamageTypeClass);

        if ((PlayerKnockbackStrength > 0.f) || !FMath::IsNearlyZero(PlayerKnockbackUp))
        {
            if (ACharacter* HitCharacter = Cast<ACharacter>(HitPawn))
            {
                FVector KnockDirection;
                if (OwnerChar.IsValid())
                {
                    KnockDirection = (HitCharacter->GetActorLocation() - OwnerChar->GetActorLocation()).GetSafeNormal2D();
                }
                        
                if (KnockDirection.IsNearlyZero())
                {
                    KnockDirection = ChargeDirection.IsNearlyZero() ? FVector::ForwardVector : ChargeDirection.GetSafeNormal2D();
                }
                        
                FVector LaunchVelocity = KnockDirection * PlayerKnockbackStrength;
                LaunchVelocity.Z += PlayerKnockbackUp;
                HitCharacter->LaunchCharacter(LaunchVelocity, /*bXYOverride=*/true, /*bZOverride=*/true);
                
                // 플레이어 히트 델리게이트 브로드캐스트
                OnPlayerHitByCharge.Broadcast(HitCharacter, KnockDirection, PlayerKnockbackStrength, OwnerChar.Get()); 
            }
        }
    }
    EndChargingInternal(EChargeEndReason::HitPawn, Other);
}
    

bool UCTankerChargeComponent::SweepAhead(FHitResult& OutHit, float Distance) const
{
    if (!OwnerChar.IsValid())
    {
        return false;
    }
    UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }
 
    const FVector Start = OwnerChar->GetActorLocation();
    const FVector Direction = ChargeDirection.IsNearlyZero() ? OwnerChar->GetActorForwardVector().GetSafeNormal2D() : ChargeDirection;
    const FVector End = Start + Direction * Distance;
   
    FCollisionQueryParams Params(SCENE_QUERY_STAT(TankerCharge), false, OwnerChar.Get());
    const FCollisionShape Capsule = FCollisionShape::MakeCapsule(TraceRadius, OwnerChar->GetSimpleCollisionHalfHeight());
   
    if (World->SweepSingleByChannel(OutHit, Start, End, FQuat::Identity, ECC_Pawn, Capsule, Params))
    {
        return true;
    }
  
    return World->SweepSingleByChannel(OutHit, Start, End, FQuat::Identity, ECC_WorldStatic, Capsule, Params);
}
    
bool UCTankerChargeComponent::EnsureOwnerAndMovement()
{
    if (!OwnerChar.IsValid())
    {
        OwnerChar = Cast<ACharacter>(GetOwner());
        if (OwnerChar.IsValid())
        {
            MoveComp = OwnerChar->GetCharacterMovement();
            CachedAI = Cast<AAIController>(OwnerChar->GetController());
        }
    }
  
    if (OwnerChar.IsValid() && !MoveComp.IsValid())
    {
        MoveComp = OwnerChar->GetCharacterMovement();
    }
   
    if (OwnerChar.IsValid() && !CachedAI.IsValid())
    {
        CachedAI = Cast<AAIController>(OwnerChar->GetController());
    }
 
    return OwnerChar.IsValid() && MoveComp.IsValid();
}

bool UCTankerChargeComponent::HasValidTarget() const
{
    return TargetActor.IsValid() && !TargetActor->IsPendingKill();
}

float UCTankerChargeComponent::DistanceToTarget2D() const
{
    if (!OwnerChar.IsValid() || !TargetActor.IsValid())
    {
        return TNumericLimits<float>::Max();
    }
   
    return FVector::Dist2D(OwnerChar->GetActorLocation(), TargetActor->GetActorLocation());
}

void UCTankerChargeComponent::FaceTowards(const FVector& Direction, float DeltaSeconds)
{
    if (!OwnerChar.IsValid())
    {
        return;
    }
 
    FRotator Current = OwnerChar->GetActorRotation();
    const FRotator TargetYaw(0.f, Direction.Rotation().Yaw, 0.f);
    Current.Yaw = FMath::FixedTurn(Current.Yaw, TargetYaw.Yaw, TurnRateDegPerSec * DeltaSeconds);
    OwnerChar->SetActorRotation(Current);
}