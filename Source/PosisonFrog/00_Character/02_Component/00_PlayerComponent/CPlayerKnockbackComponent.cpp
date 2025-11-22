#include "CPlayerKnockbackComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"

#include "04_Skill/CSkill_SpinAttack.h"
#include "00_Character/00_Player/02_Weapon/CHammer.h"
#include "00_Character/02_Component/00_PlayerComponent/CPlayerWeaponComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "00_Character/00_Player/CPlayerCharacter.h"


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
        CachedWeaponComponent = OwnerCharacter->FindComponentByClass<UCPlayerWeaponComponent>();
        CachedSpinSkill = OwnerCharacter->FindComponentByClass<UCSkill_SpinAttack>();
    }
}

void UCPlayerKnockbackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    CancelKnockback();
    OwnerCharacter.Reset();
    CachedPC.Reset();
    CachedWeaponComponent.Reset();
    CachedSpinSkill.Reset();
    
    Super::EndPlay(EndPlayReason);
}

void UCPlayerKnockbackComponent::StartKnockback(AActor* Attacker)
{
    if (ACPlayerCharacter* PC = Cast<ACPlayerCharacter>(OwnerCharacter.Get()))
    {
        PC->PlayKnockBackSound();
    }
    
    if (!OwnerCharacter.IsValid() || bIsKnockedBack)
    {
        return;
    }

    if (Attacker)
    {
        FaceAttacker(Attacker);
    }

    bIsKnockedBack = true;
    bIsStunned = false;

    if (bBlockInputDuringKnockback)
    {
        BlockInput();
    }

    PlayAirAnimation();

    if (!CachedSpinSkill.IsValid() && OwnerCharacter.IsValid())
    {
        CachedSpinSkill = OwnerCharacter->FindComponentByClass<UCSkill_SpinAttack>();
    }

    if (CachedSpinSkill.IsValid())
    {
        CachedSpinSkill->StopSpin();
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            TH_TransitionToDown,
            this,
            &UCPlayerKnockbackComponent::TransitionToDown,
            AirDuration,
            false);
    }
}

void UCPlayerKnockbackComponent::CancelKnockback()
{
    if (!bIsKnockedBack)
    {
        return;
    }

    ClearTimers();

    bIsKnockedBack = false;
    bIsStunned = false;

    UnblockInput();

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

    ACHammer* Hammer = GetHammer();
    if (Hammer)
    {
        if (USkeletalMeshComponent* HammerMesh = Hammer->GetHammerMesh())
        {
            if (UAnimInstance* HammerAnimInst = HammerMesh->GetAnimInstance())
            {
                HammerAnimInst->Montage_Stop(0.2f);
            }
        }
    }
}

void UCPlayerKnockbackComponent::PlayAirAnimation()
{
    if (!CharacterAirMontage || !HammerAirMontage)
    {
        return;
    }

    if (OwnerCharacter.IsValid())
    {
        USkeletalMeshComponent* CharMesh = OwnerCharacter->GetMesh();
        if (CharMesh)
        {
            UAnimInstance* CharAnimInst = CharMesh->GetAnimInstance();
            if (CharAnimInst)
            {
                CharAnimInst->Montage_Stop(0.1f);

                if (bLoopAirAnimation)
                {
                    CharAnimInst->Montage_Play(CharacterAirMontage, AirAnimationPlayRate);
                    CharAnimInst->Montage_SetNextSection(FName("Default"), FName("Default"), CharacterAirMontage);
                }
                else
                {
                    CharAnimInst->Montage_Play(CharacterAirMontage, AirAnimationPlayRate);
                }
            }
        }
    }

    ACHammer* Hammer = GetHammer();
    if (Hammer)
    {
        USkeletalMeshComponent* HammerMesh = Hammer->GetHammerMesh();
        if (HammerMesh)
        {
            UAnimInstance* HammerAnimInst = HammerMesh->GetAnimInstance();
            if (HammerAnimInst)
            {
                HammerAnimInst->Montage_Stop(0.1f);

                if (bLoopAirAnimation)
                {
                    HammerAnimInst->Montage_Play(HammerAirMontage, AirAnimationPlayRate);
                    HammerAnimInst->Montage_SetNextSection(FName("Default"), FName("Default"), HammerAirMontage);
                }
                else
                {
                    HammerAnimInst->Montage_Play(HammerAirMontage, AirAnimationPlayRate);
                }
            }
        }
    }
}

void UCPlayerKnockbackComponent::TransitionToDown()
{
    if (!bIsKnockedBack)
    {
        return;
    }

    bIsStunned = true;
    StopMovement();

    if (CharacterDownMontage && OwnerCharacter.IsValid())
    {
        if (USkeletalMeshComponent* CharMesh = OwnerCharacter->GetMesh())
        {
            if (UAnimInstance* CharAnimInst = CharMesh->GetAnimInstance())
            {
                CharAnimInst->Montage_Stop(0.0f);
                CharAnimInst->Montage_Play(CharacterDownMontage, 1.0f);
            }
        }
    }

    ACHammer* Hammer = GetHammer();
    if (HammerDownMontage && Hammer)
    {
        if (USkeletalMeshComponent* HammerMesh = Hammer->GetHammerMesh())
        {
            if (UAnimInstance* HammerAnimInst = HammerMesh->GetAnimInstance())
            {
                HammerAnimInst->Montage_Stop(0.0f);
                HammerAnimInst->Montage_Play(HammerDownMontage, 1.0f);
            }
        }
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            TH_StunEnd,
            this,
            &UCPlayerKnockbackComponent::OnStunEnd,
            StunDuration,
            false);
    }
}

void UCPlayerKnockbackComponent::OnStunEnd()
{
    if (!bIsKnockedBack)
    {
        return;
    }

    float GetUpLength = 0.f;

    if (CharacterGetUpMontage && OwnerCharacter.IsValid())
    {
        PlayMontage(CharacterGetUpMontage);
        GetUpLength = FMath::Max(GetUpLength, CharacterGetUpMontage->GetPlayLength());
    }

    ACHammer* Hammer = GetHammer();
    if (HammerGetUpMontage && Hammer)
    {
        if (USkeletalMeshComponent* HammerMesh = Hammer->GetHammerMesh())
        {
            if (UAnimInstance* HammerAnimInst = HammerMesh->GetAnimInstance())
            {
                HammerAnimInst->Montage_Stop(0.1f);
                HammerAnimInst->Montage_Play(HammerGetUpMontage, 1.0f);
                GetUpLength = FMath::Max(GetUpLength, HammerGetUpMontage->GetPlayLength());
            }
        }
    }

    if (GetUpLength > 0.f)
    {
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
        OnGetUpComplete();
    }
}

void UCPlayerKnockbackComponent::OnGetUpComplete()
{
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
    }
}

void UCPlayerKnockbackComponent::UnblockInput()
{
    if (CachedPC.IsValid() && OwnerCharacter.IsValid())
    {
        CachedPC->EnableInput(CachedPC.Get());
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

    const FVector ToPlayer = OwnerCharacter->GetActorLocation() - Attacker->GetActorLocation();
    const FVector DirectionToPlayer = ToPlayer.GetSafeNormal2D();

    if (!DirectionToPlayer.IsNearlyZero())
    {
        const FRotator NewRotation = DirectionToPlayer.Rotation();
        OwnerCharacter->SetActorRotation(FRotator(0.f, NewRotation.Yaw + 180.f, 0.f));
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

ACHammer* UCPlayerKnockbackComponent::GetHammer() const
{
    if (!CachedWeaponComponent.IsValid() && OwnerCharacter.IsValid())
    {
        UCPlayerKnockbackComponent* MutableThis = const_cast<UCPlayerKnockbackComponent*>(this);
        MutableThis->CachedWeaponComponent = OwnerCharacter->FindComponentByClass<UCPlayerWeaponComponent>();
    }
    
    if (CachedWeaponComponent.IsValid())
    {
        return CachedWeaponComponent->GetHammer();
    }
    
    return nullptr;
}