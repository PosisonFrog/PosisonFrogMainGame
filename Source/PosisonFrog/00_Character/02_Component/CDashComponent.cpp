#include "CDashComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UCDashComponent::UCDashComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetComponentTickEnabled(false);
    
}

void UCDashComponent::BeginPlay()
{
    Super::BeginPlay();
    OwnerChar = Cast<ACharacter>(GetOwner());
    if (OwnerChar)
        MoveComp = OwnerChar->GetCharacterMovement();
}

void UCDashComponent::StartDash()
{
    if (!OwnerChar || !MoveComp) return; 
    if (bIsDashing) return;
    if (bIsOnCoolDown) return;

    BeginDash_Internal();
}

void UCDashComponent::BeginDash_Internal()
{
    // 전방 수평 방향(카메라가 아닌 캐릭터 방향 기준)
    DashDir2D = OwnerChar->GetActorForwardVector();
    DashDir2D.Z = 0.f;
    DashDir2D = DashDir2D.GetSafeNormal();
    if (DashDir2D.IsNearlyZero())
        DashDir2D = FVector::ForwardVector;

    // 물리 파라미터 저장 & 오버라이드
    ApplyPhysicsOverrides();

    // Z 속도 제거(옵션)
    if (bClearZVelocity)
    {
        FVector V = MoveComp->Velocity;
        V.Z = 0.f;
        MoveComp->Velocity = V;
    }

    // 대시 시작
    bIsDashing = true;
    DashTimeAcc = 0.f;
    SetComponentTickEnabled(true);
}

void UCDashComponent::EndDash_Internal()
{
    bIsDashing = false;
    SetComponentTickEnabled(false);
    
    // 대시 종료 시 속도 감소 적용
    if (bApplyStopForceOnDashEnd && MoveComp)
    {
        FVector CurrentVel = MoveComp->Velocity;
        CurrentVel.X *= StopForceMultiplier;
        CurrentVel.Y *= StopForceMultiplier;
        // Z축(점프/낙하)는 유지
        MoveComp->Velocity = CurrentVel;
    }
    
    RestorePhysicsOverrides();
    
}

void UCDashComponent::ApplyPhysicsOverrides()
{
    // 스냅샷(기존 코드 재사용하기 위해서 기존 변수의 값을 저장.)
    Saved_GroundFriction         = MoveComp->GroundFriction;
    Saved_BrakingFrictionFactor  = MoveComp->BrakingFrictionFactor;
    Saved_BrakingDecelWalking    = MoveComp->BrakingDecelerationWalking;
    Saved_bOrientRotationToMovement = OwnerChar->bUseControllerRotationYaw ? false : MoveComp->bOrientRotationToMovement;
    Saved_DefaultMovementSpeed = MoveComp->GetMaxSpeed();
    
    // 오버라이드(낮은 마찰/제동으로 직진 손맛 강화)
    MoveComp->GroundFriction            = Override_GroundFriction;
    MoveComp->BrakingFrictionFactor     = Override_BrakingFrictionFactor;
    MoveComp->BrakingDecelerationWalking= Override_BrakingDecelWalking;

    // 대시 중에는 이동 방향 고정(선택) → 여기서는 유지, 필요 시 OrientRotationToMovement 조절 가능
}

void UCDashComponent::RestorePhysicsOverrides()
{
    if (!MoveComp) return;
    
    // 대시 종료 후 높은 마찰계수로 빠른 정지
    MoveComp->GroundFriction             = DashEndFriction; // 마찰계수로 바꿈
    MoveComp->BrakingFrictionFactor      = Saved_BrakingFrictionFactor;
    MoveComp->BrakingDecelerationWalking = DashEndBrakingDecel; // 높은 제동력 제동력으로 바꿈
   
    // 일정 시간 후 원래 값으로 복구하는 타이머 설정
    FTimerHandle RestoreTimer;
    GetWorld()->GetTimerManager().SetTimer(RestoreTimer, [this]()
    {
        if (MoveComp)
        {
            MoveComp->GroundFriction = Saved_GroundFriction;
            MoveComp->BrakingDecelerationWalking = Saved_BrakingDecelWalking;
        }
    }, 0.5f, false); // 0.5초 후 원래 값으로 복구
}


void UCDashComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!bIsDashing || !MoveComp) return; 

    DashTimeAcc += DeltaTime;

    // 대시 중 속도 유지(수평)
    FVector Vel = DashDir2D * DashSpeed;
    if (!MoveComp->IsMovingOnGround())
    {
        // 공중이라면 수평만 유지하고 Z는 기존 유지
        Vel.Z = MoveComp->Velocity.Z;
    }
    MoveComp->Velocity = Vel;

    // 끝나면 원복
    if (DashTimeAcc >= DashDuration)
        EndDash_Internal();
}
