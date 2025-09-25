#include "CDashComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

UCDashComponent::UCDashComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetComponentTickEnabled(false);
}

void UCDashComponent::BeginPlay()
{
    Super::BeginPlay();
    OwnerChar = Cast<ACharacter>(GetOwner());
    if (OwnerChar.IsValid())
        MoveComp = OwnerChar->GetCharacterMovement();
}


void UCDashComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
        World->GetTimerManager().ClearTimer(TimerHandle_DelayedRestore);

    SetComponentTickEnabled(false);
    Super::EndPlay(EndPlayReason);
}


void UCDashComponent::StartDash()
{
    if (!OwnerChar.IsValid() || !MoveComp.IsValid() || bIsDashing)
        return;

    BeginDash_Internal();
}
void UCDashComponent::CancelDash()
{
    if (bIsDashing)
        EndDash_Internal();
}


FVector UCDashComponent::ResolveDashDirection() const
{
    // 외부 지정 방향 우선
    if (bUseExplicitDir && !ExplicitDir.IsNearlyZero())
    {
        FVector D = ExplicitDir;
        D.Z = 0.f;
        
        return D.GetSafeNormal();
    }

    FVector Dir = OwnerChar->GetActorForwardVector();

    if (bUseCameraYaw)
    {
        if (AController* C = OwnerChar->GetController())
        {
            const FRotator YawRot(0.f, C->GetControlRotation().Yaw, 0.f);
            Dir = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
        }
    }

    Dir.Z = 0.f;
    if (Dir.IsNearlyZero()) Dir = FVector::ForwardVector;
    return Dir.GetSafeNormal();
}

void UCDashComponent::BeginDash_Internal()
{
    DashDir2D = ResolveDashDirection();

    ApplyPhysicsOverrides();

    // 시작 시 Z 제거(옵션)
    if (bClearZVelocity && MoveComp.IsValid())
    {
        FVector V = MoveComp->Velocity; V.Z = 0.f;
        MoveComp->Velocity = V;
    }

    // Launch 모드: 한 번에 추진
    if (bUseLaunchMode && OwnerChar.IsValid())
    {
        const FVector Impulse = DashDir2D * LaunchStrength + FVector::UpVector * LaunchUpward;
        OwnerChar->LaunchCharacter(Impulse, /*XYOverride=*/true, /*ZOverride=*/true);
    }

    bIsDashing = true;
    DashTimeAcc = 0.f;
    SetComponentTickEnabled(true);

    OnDashStarted.Broadcast();
}

void UCDashComponent::EndDash_Internal()
{
    SetComponentTickEnabled(false);

    // 종료 즉시 수평 감속감
    if (MoveComp.IsValid() && bApplyStopForceOnEnd)
    {
        FVector V = MoveComp->Velocity;
        V.X *= StopForceMultiplier;
        V.Y *= StopForceMultiplier;
        MoveComp->Velocity = V;
    }

    // 즉시 일부 → 잠시 후 완전 복구
    RestorePhysicsOverrides_Immediate();

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(TimerHandle_DelayedRestore);
        World->GetTimerManager().SetTimer(
            TimerHandle_DelayedRestore,
            this, &UCDashComponent::RestorePhysicsOverrides_Delayed,
            0.5f, false);
    }

    // 외부 지정 방향 리셋
    bUseExplicitDir = false;
    bIsDashing = false;
    
    ExplicitDir = FVector::ZeroVector;
    OnDashEnded.Broadcast();
    
}

void UCDashComponent::ApplyPhysicsOverrides()
{
    if (!MoveComp.IsValid()) return;

    // 스냅샷
    Saved_GroundFriction = MoveComp->GroundFriction;
    Saved_BrakingFrictionFactor = MoveComp->BrakingFrictionFactor;
    Saved_BrakingDecelWalking = MoveComp->BrakingDecelerationWalking;
    Saved_bOrientRotationToMovement = MoveComp->bOrientRotationToMovement;

    // 오버라이드
    MoveComp->GroundFriction = Override_GroundFriction;
    MoveComp->BrakingFrictionFactor = Override_BrakingFrictionFactor;
    MoveComp->BrakingDecelerationWalking = Override_BrakingDecelWalking;
}

void UCDashComponent::RestorePhysicsOverrides_Immediate()
{
    if (!MoveComp.IsValid()) return;

    // 종료 직후 빠르게 붙는 감속감
    MoveComp->GroundFriction = End_GroundFriction;
    MoveComp->BrakingFrictionFactor = Saved_BrakingFrictionFactor; // 즉시 원복
    MoveComp->BrakingDecelerationWalking = End_BrakingDecelWalking;
}
void UCDashComponent::RestorePhysicsOverrides_Delayed()
{
    if (!MoveComp.IsValid()) return;

    MoveComp->GroundFriction = Saved_GroundFriction;
    MoveComp->BrakingDecelerationWalking = Saved_BrakingDecelWalking;
    MoveComp->bOrientRotationToMovement = Saved_bOrientRotationToMovement;
}

void UCDashComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bIsDashing || !MoveComp.IsValid())
        return;

    DashTimeAcc += DeltaTime;

    // Launch 모드가 아닌 경우: 수평 속도 직접 유지
    if (!bUseLaunchMode)
    {
        float Speed = DashSpeed;

        if (SpeedCurve)
        {
            const float T = FMath::Clamp(DashTimeAcc / FMath::Max(0.01f, DashDuration), 0.f, 1.f);
            Speed *= FMath::Max(0.f, SpeedCurve->GetFloatValue(T));
        }

        FVector V = DashDir2D * Speed;
        if (!MoveComp->IsMovingOnGround())
            V.Z = MoveComp->Velocity.Z; // 공중은 Z 유지

        MoveComp->Velocity = V;
    }

    if (DashTimeAcc >= DashDuration)
        EndDash_Internal();
}
