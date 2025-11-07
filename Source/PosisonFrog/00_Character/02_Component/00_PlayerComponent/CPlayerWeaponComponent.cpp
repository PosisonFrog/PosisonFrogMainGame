#include "00_Character/02_Component/00_PlayerComponent/CPlayerWeaponComponent.h"
#include "00_Character/00_Player/CPlayerCharacter.h"
#include "00_Character/00_Player/02_Weapon/CHammer.h"
#include "00_Character/02_Component/00_PlayerComponent/Buffable.h"
#include "00_Character/02_Component/01_EnemyComponent/CEnemyHealthComponent.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "99_Util/CLog.h"
#include "AnimNodes/AnimNode_RandomPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"


class IBuffable;

UCPlayerWeaponComponent::UCPlayerWeaponComponent()
{
    AttachSocketName = TEXT("Hand_Hammer");
}

void UCPlayerWeaponComponent::SpawnWeapon()
{
    UWorld* World = GetWorld();
    if (!IsValid(World) || !OwnerChar.IsValid())
        return;
    

    FActorSpawnParameters Params;
    Params.Owner = OwnerChar.Get();
    Params.Instigator = OwnerChar.Get();
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    CurrentWeapon = World->SpawnActor<ACHammer>(
        WeaponClass,
        OwnerChar->GetActorLocation(),
        OwnerChar->GetActorRotation(),
        Params);
    
    if (!IsValid(CurrentWeapon))
    {
        CLog::Log(TEXT("[WeaponComp] SpawnWeapon failed"));
        return;
    }

    CurrentWeapon->DeactivateDamage();
    AttachWeaponToCharacter();
}

void UCPlayerWeaponComponent::HandleWeaponHit(AActor* InstigatorActor, AActor* HitActor, float Damage, FHitResult Hit)
{
    if (!IsValid(HitActor) || !IsValid(InstigatorActor))
        return;

    if (HitActor == OwnerChar)
        return;
    
    
    if (ACPlayerCharacter* PlayerChar = Cast<ACPlayerCharacter>(GetOwner()))
    {
        const float gain = PlayerChar->GetMaxUltimateGauge() * AddUltGaugeMul;
        PlayerChar->AddUltimateGain(gain);
    }
    
    // 콤보 배율 적용
    float ComboMultiplier = 1.0f;
    if (ComboAttackRatio.IsValidIndex(CurrentCombo))
    {
        ComboMultiplier = ComboAttackRatio[CurrentCombo];
    }
    
    float CurrentDamage = Damage * ComboMultiplier;
    float FinalDamage = CurrentDamage;

    if (IBuffable* Buffable = Cast<IBuffable>(OwnerChar))
    {
        if (Buffable->IsBuffActive())
        {
            const float Multiplier = Buffable->GetOutgoingDamageMultiplier();
            FinalDamage *= Multiplier;

            if (Multiplier != 1.0f)
                UE_LOG(LogTemp,Log,TEXT("[BaseWeaponComp] Damage scale : %.1f -> %.1f (x%.2f)"), Damage, FinalDamage, Multiplier);
        }
    }

    Super::HandleWeaponHit(InstigatorActor, HitActor, FinalDamage, Hit);

    UE_LOG(LogTemp, Warning, TEXT("[WeaponComp] About to broadcast combo hit: Actor=%s, Combo=%d, Damage=%.1f, Bound=%d"), 
         *HitActor->GetName(), CurrentCombo, FinalDamage, OnPlayerComboHit.IsBound() ? 1 : 0);
    
    if (OnPlayerComboHit.IsBound())
    {
        OnPlayerComboHit.Broadcast(HitActor, CurrentCombo, FinalDamage);
        UE_LOG(LogTemp, Warning, TEXT("[WeaponComp] Broadcast complete"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[WeaponComp] OnPlayerComboHit not bound!"));
    }

    if (bEnableHitKnockback && HitKnockbackStrength > 0.f)
    {
        if (ACharacter* HitCharacter = Cast<ACharacter>(HitActor))
        {
            if (HitCharacter->FindComponentByClass<UCEnemyHealthComponent>())
            {
                FVector KnockDirection = FVector::ZeroVector;
                           
                if (Hit.bBlockingHit)
                {
                    KnockDirection = -Hit.ImpactNormal;
                    KnockDirection.Z = 0.f;
                }
                           
                if (KnockDirection.IsNearlyZero() && OwnerChar.IsValid())
                {
                    KnockDirection = HitCharacter->GetActorLocation() - OwnerChar->GetActorLocation();
                    KnockDirection.Z = 0.f;
                }
                           
                if (KnockDirection.IsNearlyZero())
                {
                    KnockDirection = HitCharacter->GetActorForwardVector();
                    KnockDirection.Z = 0.f;
                }
                         
                KnockDirection = KnockDirection.GetSafeNormal();
                if (!KnockDirection.IsNearlyZero())
                {
                    FVector LaunchVelocity = KnockDirection * HitKnockbackStrength;
                    if (HitKnockbackUpStrength > 0.f)
                    {
                        LaunchVelocity.Z += HitKnockbackUpStrength;
                    }
                                   
                    HitCharacter->LaunchCharacter(LaunchVelocity, true, HitKnockbackUpStrength > 0.f);
                }
            }
        }
    }
}

/* ============ 공격/콤보 ============ */
void UCPlayerWeaponComponent::DoAttack()
{
    if (!OwnerChar.IsValid() || PlayerComboMontages.Num() == 0)
        return;

    if (!IsValid(CurrentWeapon) || HammerComboMontages.Num() == 0)
        return;

    if (ACPlayerCharacter* PlayerOwner = Cast<ACPlayerCharacter>(OwnerChar.Get()))
    {
        PlayerOwner->SetAttackMovementSlowMultiplier(AttackMoveSpeedMul);
    }

    bHasNotifiedAttackEnd = false;

    // 이미 공격 중: 창이 열려 있으면 즉시 다음 스텝, 아니면 큐잉
    if (bIsAttacking)
    {
        if (bCanNextCombo 
                   && CurrentCombo < PlayerComboMontages.Num() - 1 
                   && CurrentCombo < HammerComboMontages.Num() - 1
                   && CurrentCombo < ComboAttackRatio.Num() - 1)
        {
       	    StepToNextCombo();
       	}
        else
        {
            bQueuedNextInput = true; // 창 열릴 때 자동 처리
        }
        return;
    }

    // 첫타 시작
    bIsAttacking = true;
    bQueuedNextInput = false;
    CurrentCombo = 0;
    PlayComboAttack();
}

void UCPlayerWeaponComponent::PlayComboAttack()
{
    if (!PlayerComboMontages.IsValidIndex(CurrentCombo))
    {
        CLog::Log(TEXT("[WeaponComp] PlayComboAttack: invalid index"));
        ResetCombo();
        return;
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("[PlayerComboAttack] index : %d"), CurrentCombo);
    }

    UAnimMontage* PlayerMontage = PlayerComboMontages[CurrentCombo];
    UAnimMontage* HammerMontage = HammerComboMontages[CurrentCombo];

    if (!IsValid(PlayerMontage))
    {
        CLog::Log(TEXT("[WeaponComp] PlayComboAttack: Player montage null"));
        ResetCombo();
        return;
    }

    if (!IsValid(HammerMontage))
    {
        CLog::Log(TEXT("[WeaponComp] PlayComboAttack: Hammer montage null"));
        ResetCombo();
        return;
    }

    UAnimInstance* PlayerAnimInst = (OwnerChar.Get() && OwnerChar->GetMesh())
        ? OwnerChar->GetMesh()->GetAnimInstance()
        : nullptr;

    ACHammer* Hammer = GetHammer();
    UAnimInstance* HammerAnimInst = (Hammer && Hammer->GetHammerMesh())
        ? Hammer->GetHammerMesh()->GetAnimInstance()
        : nullptr;
    
    if (!PlayerAnimInst || !HammerAnimInst)
    {
        CLog::Log(TEXT("[WeaponComp] PlayComboAttack: AnimInstance null"));
        ResetCombo();
        return;
    }
    
    PlayerAnimInst->Montage_Play(PlayerMontage);
    HammerAnimInst->Montage_Play(HammerMontage);

    /*if (IsValid(Hammer))
        Hammer->PlayAttackVFX(CurrentCombo);*/
    
    // 종료시 정리(인터럽트/블렌드아웃 포함)
    FOnMontageEnded EndDelegate;
    EndDelegate.BindUObject(this, &UCPlayerWeaponComponent::OnMontageEnded);
    PlayerAnimInst->Montage_SetEndDelegate(EndDelegate, PlayerMontage);
    
    // 콤보 리셋 타이머
    GetWorld()->GetTimerManager().ClearTimer(ComboResetTimer);
    GetWorld()->GetTimerManager().SetTimer(ComboResetTimer, this, &UCPlayerWeaponComponent::ResetCombo, ComboResetTime, false);
}

void UCPlayerWeaponComponent::StepToNextCombo()
{
    if (CurrentCombo >= PlayerComboMontages.Num() - 1)
        return;

    ++CurrentCombo;
    bQueuedNextInput = false; // 소비
    bCanNextCombo = false;    // 창 닫힘으로 간주(다음 애님에서 다시 열림)
    
    PlayComboAttack();
}

void UCPlayerWeaponComponent::ResetCombo()
{
    if (!bIsAttacking)
        return;

    GetWorld()->GetTimerManager().ClearTimer(ComboResetTimer);

    if (ACharacter* Ch = OwnerChar.Get())
    {
        if (ACPlayerCharacter* PC = Cast<ACPlayerCharacter>(Ch))
        {
            if (!bHasNotifiedAttackEnd)
            {
                PC->OnAttackEnded();
                bHasNotifiedAttackEnd = true;
            }
        }
    }

    bIsAttacking = false;
    bCanNextCombo = false;
    bQueuedNextInput = false;
    CurrentCombo = 0;

    // 안전: 히트창 닫기
    DisableAttackBoxCollider();
}



void UCPlayerWeaponComponent::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    if (bInterrupted)
        return;
    
    ResetCombo();

    if (bInterrupted)
    {
        DisableAttackBoxCollider();
    }
}

/* ============ 상태기/노티에서 호출 ============ */
void UCPlayerWeaponComponent::BeginAction()
{
    bIsAttacking = true;
    
    if (ACharacter* Ch = OwnerChar.Get())
    {
        if (ACPlayerCharacter* PC = Cast<ACPlayerCharacter>(Ch))
            PC->OnAttackStarted();
    }
}
void UCPlayerWeaponComponent::EndAction()
{
    //ResetCombo();
    if (ACharacter* Ch = OwnerChar.Get())
    {
        if (ACPlayerCharacter* PC = Cast<ACPlayerCharacter>(Ch))
        {
            PC->OnAttackEnded();
            bHasNotifiedAttackEnd = true;
        }

    }
}

void UCPlayerWeaponComponent::EnableComboInput()
{
    bCanNextCombo = true;

    // 입력이 미리 들어와 있으면 즉시 다음 스텝으로
    if (bQueuedNextInput && bIsAttacking && CurrentCombo < PlayerComboMontages.Num() - 1)
    {
        StepToNextCombo();
    }
}

void UCPlayerWeaponComponent::DisableComboInput()
{
    bCanNextCombo = false;
}