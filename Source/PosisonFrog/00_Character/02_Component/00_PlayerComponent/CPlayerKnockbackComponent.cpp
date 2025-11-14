#include "CPlayerKnockbackComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"


UCPlayerKnockbackComponent::UCPlayerKnockbackComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UCPlayerKnockbackComponent::BeginPlay()
{
    Super::BeginPlay();

    OwnerCharacter = Cast<ACharacter>(GetOwner());
    if (OwnerCharacter.IsValid())
    {
        CachedPC = Cast<APlayerController>(OwnerCharacter->GetController());
    }

    if (bDebugLog)
    {
        UE_LOG(LogTemp, Log, TEXT("[KnockbackComponent] Initialized for %s"), *GetNameSafe(GetOwner()));
    }
}

void UCPlayerKnockbackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    CancelKnockback();
    OwnerCharacter.Reset();
    CachedPC.Reset();
    
    Super::EndPlay(EndPlayReason);
}

void UCPlayerKnockbackComponent::StartKnockback(AActor* Attacker)
{
    if (!OwnerCharacter.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[KnockbackComponent] StartKnockback failed: No owner character"));
        return;
    }

    if (bIsKnockedBack)
    {
        if (bDebugLog)
        {
            UE_LOG(LogTemp, Warning, TEXT("[KnockbackComponent] Already in knockback state"));
        }
        return;
    }

    // 공격자를 향해 회전
    if (Attacker)
    {
        FaceAttacker(Attacker);
    }

    bIsKnockedBack = true;
    bIsStunned = false;

    if (bDebugLog)
    {
        UE_LOG(LogTemp, Log, TEXT("[KnockbackComponent] ===== Knockback Sequence Started ====="));
    }

    if (bBlockInputDuringKnockback)
    {
        BlockInput();
    }

    PlayAirAnimation();

    // AirDuration 후 자동으로 기절
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            TH_TransitionToDown,
            this,
            &UCPlayerKnockbackComponent::TransitionToDown,
            AirDuration,
            false);
            
        if (bDebugLog)
        {
            UE_LOG(LogTemp, Log, TEXT("[KnockbackComponent] Transition to DOWN scheduled in %.2f seconds"), AirDuration);
        }
    }
}

void UCPlayerKnockbackComponent::CancelKnockback()
{
    if (!bIsKnockedBack)
    {
        return;
    }

    if (bDebugLog)
    {
        UE_LOG(LogTemp, Warning, TEXT("[KnockbackComponent] Knockback cancelled"));
    }

    ClearTimers();

    bIsKnockedBack = false;
    bIsStunned = false;

    UnblockInput();

    // 애니메이션 중단
    if (OwnerCharacter.IsValid())
    {
        if (USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh())
        {
            if (UAnimInstance* AnimInst = Mesh->GetAnimInstance())
            {
                AnimInst->Montage_Stop(0.2f);
            }
        }
    }
}

void UCPlayerKnockbackComponent::PlayAirAnimation()
{
    if (!AirMontage)
    {
        if (bDebugLog)
        {
            UE_LOG(LogTemp, Warning, TEXT("[KnockbackComponent] AirMontage not set"));
        }
        return;
    }

    if (!OwnerCharacter.IsValid())
    {
        return;
    }

    USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh();
    if (!Mesh)
    {
        return;
    }

    UAnimInstance* AnimInst = Mesh->GetAnimInstance();
    if (!AnimInst)
    {
        return;
    }

    // 기존 몽타주 중단
    AnimInst->Montage_Stop(0.1f);

    // 공중 애니메이션 재생 (루프 설정 가능)
    if (bLoopAirAnimation)
    {
        AnimInst->Montage_Play(AirMontage, AirAnimationPlayRate);
        // 루프 섹션 설정 (몽타주에 "Default" 섹션이 있어야 함, 없으면 무시됨)
        AnimInst->Montage_SetNextSection(FName("Default"), FName("Default"), AirMontage);
    }
    else
    {
        AnimInst->Montage_Play(AirMontage, AirAnimationPlayRate);
    }

    if (bDebugLog)
    {
        UE_LOG(LogTemp, Log, TEXT("[KnockbackComponent] [1/3] Playing AIR animation (Loop: %s, PlayRate: %.2f)"), 
            bLoopAirAnimation ? TEXT("Yes") : TEXT("No"), AirAnimationPlayRate);
    }
}

void UCPlayerKnockbackComponent::TransitionToDown()
{
    if (!bIsKnockedBack)
    {
        return;
    }

    bIsStunned = true;

    if (bDebugLog)
    {
        UE_LOG(LogTemp, Log, TEXT("[KnockbackComponent] [2/3] Transitioning to DOWN - Starting stun phase"));
    }

    // 이동 완전히 중지
    StopMovement();

    // 2단계: 기절 애니메이션
    if (DownMontage)
    {
        if (OwnerCharacter.IsValid())
        {
            if (USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh())
            {
                if (UAnimInstance* AnimInst = Mesh->GetAnimInstance())
                {
                    // 기존 몽타주 완전히 중단
                    AnimInst->Montage_Stop(0.0f);
                    
                    // 기절 애니메이션 재생
                    AnimInst->Montage_Play(DownMontage, 1.0f);
                    
                    if (bDebugLog)
                    {
                        UE_LOG(LogTemp, Log, TEXT("[KnockbackComponent] Playing DOWN montage: %s"), *DownMontage->GetName());
                    }
                }
            }
        }
    }
    else if (bDebugLog)
    {
        UE_LOG(LogTemp, Warning, TEXT("[KnockbackComponent] DownMontage not set!"));
    }

    // 기절 지속 후 일어서기
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            TH_StunEnd,
            this,
            &UCPlayerKnockbackComponent::OnStunEnd,
            StunDuration,
            false);
            
        if (bDebugLog)
        {
            UE_LOG(LogTemp, Log, TEXT("[KnockbackComponent] Stun timer set for %.2f seconds"), StunDuration);
        }
    }
}

void UCPlayerKnockbackComponent::OnStunEnd()
{
    if (!bIsKnockedBack)
    {
        return;
    }

    if (bDebugLog)
    {
        UE_LOG(LogTemp, Log, TEXT("[KnockbackComponent] [3/3] Stun ended - Getting up"));
    }

    // 3단계: 일어서기 애니메이션
    if (GetUpMontage)
    {
        PlayMontage(GetUpMontage);

        // 일어서기 애님 길이만큼 대기 후 완전 복구
        const float GetUpLength = GetUpMontage->GetPlayLength();
        
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().SetTimer(
                TH_GetUpComplete,
                this,
                &UCPlayerKnockbackComponent::OnGetUpComplete,
                GetUpLength,
                false);
        }
    }
    else
    {
        // GetUpMontage가 없으면 바로 복구
        OnGetUpComplete();
    }
}

void UCPlayerKnockbackComponent::OnGetUpComplete()
{
    if (bDebugLog)
    {
        UE_LOG(LogTemp, Log, TEXT("[KnockbackComponent] ===== Knockback Sequence Complete ====="));
    }

    bIsKnockedBack = false;
    bIsStunned = false;

    UnblockInput();
    ClearTimers();
}

void UCPlayerKnockbackComponent::BlockInput()
{
    if (CachedPC.IsValid() && OwnerCharacter.IsValid())
    {
        CachedPC->DisableInput(CachedPC.Get());
        
        if (bDebugLog)
        {
            UE_LOG(LogTemp, Log, TEXT("[KnockbackComponent] Input BLOCKED"));
        }
    }
}

void UCPlayerKnockbackComponent::UnblockInput()
{
    if (CachedPC.IsValid() && OwnerCharacter.IsValid())
    {
        CachedPC->EnableInput(CachedPC.Get());
        
        if (bDebugLog)
        {
            UE_LOG(LogTemp, Log, TEXT("[KnockbackComponent] Input UNBLOCKED"));
        }
    }
}

void UCPlayerKnockbackComponent::StopMovement()
{
    if (OwnerCharacter.IsValid())
    {
        if (UCharacterMovementComponent* MoveComp = OwnerCharacter->GetCharacterMovement())
        {
            MoveComp->StopMovementImmediately();
            MoveComp->Velocity = FVector::ZeroVector;
        }
    }
}

void UCPlayerKnockbackComponent::PlayMontage(UAnimMontage* Montage)
{
    if (!Montage || !OwnerCharacter.IsValid())
    {
        return;
    }

    if (USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh())
    {
        if (UAnimInstance* AnimInst = Mesh->GetAnimInstance())
        {
            // 기존 몽타주 중단 후 새로 재생
            AnimInst->Montage_Stop(0.1f);
            AnimInst->Montage_Play(Montage, 1.0f);
        }
    }
}


void UCPlayerKnockbackComponent::FaceAttacker(AActor* Attacker)
{
    if (!OwnerCharacter.IsValid() || !Attacker)
    {
        return;
    }

    // 플레이어 방향 계산
    const FVector ToPlayer = OwnerCharacter->GetActorLocation() - Attacker->GetActorLocation();
    const FVector DirectionToPlayer = ToPlayer.GetSafeNormal2D();

    if (!DirectionToPlayer.IsNearlyZero())
    {
        // 플레이어를 공격자 방향으로 회전 (서로 마주보게)
        const FRotator NewRotation = DirectionToPlayer.Rotation();
        OwnerCharacter->SetActorRotation(FRotator(0.f, NewRotation.Yaw + 180.f, 0.f));

        if (bDebugLog)
        {
            UE_LOG(LogTemp, Log, TEXT("[KnockbackComponent] Rotated player to face attacker (Yaw: %.1f)"), NewRotation.Yaw);
        }
    }
}

void UCPlayerKnockbackComponent::ClearTimers()
{
    if (UWorld* World = GetWorld())
    {
        FTimerManager& TimerManager = World->GetTimerManager();
        TimerManager.ClearTimer(TH_TransitionToDown);
        TimerManager.ClearTimer(TH_StunEnd);
        TimerManager.ClearTimer(TH_GetUpComplete);
    }
}