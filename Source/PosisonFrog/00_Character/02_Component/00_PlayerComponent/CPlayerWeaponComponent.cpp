#include "00_Character/02_Component/00_PlayerComponent/CPlayerWeaponComponent.h"

#include "CPlayerEffectComponent.h"
#include "00_Character/00_Player/CPlayerCharacter.h"
#include "00_Character/00_Player/02_Weapon/CHammer.h"
#include "00_Character/02_Component/00_PlayerComponent/Buffable.h"
#include "00_Character/02_Component/01_EnemyComponent/CEnemyHealthComponent.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "TimerManager.h"
#include "00_Character/02_Component/CHitStopComponent.h"
#include "Engine/World.h"
#include "99_Util/CLog.h"
#include "Components/CapsuleComponent.h"


class IBuffable;

UCPlayerWeaponComponent::UCPlayerWeaponComponent()
{
    AttachSocketName = TEXT("Hand_Hammer");

    HitStopComponent = CreateDefaultSubobject<UCHitStopComponent>(TEXT("HitStopComponent"));
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
    

    // 궁극기 게이지 증가 로직 제거
    /*if (ACPlayerCharacter* PlayerChar = Cast<ACPlayerCharacter>(GetOwner()))
    {
        const float gain = PlayerChar->GetMaxUltimateGauge() * AddUltGaugeMul;
        PlayerChar->AddUltimateGain(gain);
    }*/
    
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
                UE_LOG(LogTemp, Log, TEXT("[BaseWeaponComp] Damage scale : %.1f -> %.1f (x%.2f)"), Damage, FinalDamage, Multiplier);
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

    if (bEnableHitStop && IsValid(HitStopComponent) && CurrentCombo == 2 && !bHitStopTriggeredThisCombo)
    {
        bHitStopTriggeredThisCombo = true;
        
        TArray<AActor*> HitStopTargets;
        
        if (OwnerChar.IsValid())
            HitStopTargets.Add(OwnerChar.Get());
        if (IsValid(HitActor))
            HitStopTargets.Add(HitActor);
        if (IsValid(CurrentWeapon))
            HitStopTargets.Add(CurrentWeapon);

        // 플레이어 애니메이션 명시적 정지
        if (OwnerChar.IsValid() && OwnerChar->GetMesh())
        {
            UAnimInstance* PlayerAnimInst = OwnerChar->GetMesh()->GetAnimInstance();
            if (PlayerAnimInst)
            {
                if (UAnimMontage* CurrentMontage = PlayerAnimInst->GetCurrentActiveMontage())
                {
                    PlayerAnimInst->Montage_Pause(CurrentMontage);
                    
                    // 히트스톱 종료 후 재개
                    FTimerHandle ResumeTimer;
                    GetWorld()->GetTimerManager().SetTimer(
                        ResumeTimer,
                        [PlayerAnimInst, CurrentMontage]()
                        {
                            if (IsValid(PlayerAnimInst) && IsValid(CurrentMontage))
                            {
                                PlayerAnimInst->Montage_Resume(CurrentMontage);
                            }
                        },
                        ThirdComboHitStopDuration,
                        false
                    );
                }
            }
        }

        // 해머 애니메이션 명시적 정지
        ACHammer* Hammer = GetHammer();
        if (IsValid(Hammer) && Hammer->GetHammerMesh())
        {
            UAnimInstance* HammerAnimInst = Hammer->GetHammerMesh()->GetAnimInstance();
            if (HammerAnimInst)
            {
                // 현재 재생 중인 몽타주 일시정지
                if (UAnimMontage* CurrentMontage = HammerAnimInst->GetCurrentActiveMontage())
                {
                    HammerAnimInst->Montage_Pause(CurrentMontage);
                    
                    // 히트스톱 종료 후 재개하기 위한 타이머
                    FTimerHandle ResumeTimer;
                    GetWorld()->GetTimerManager().SetTimer(
                        ResumeTimer,
                        [HammerAnimInst, CurrentMontage]()
                        {
                            if (IsValid(HammerAnimInst) && IsValid(CurrentMontage))
                            {
                                HammerAnimInst->Montage_Resume(CurrentMontage);
                            }
                        },
                        ThirdComboHitStopDuration,
                        false
                    );
                }
            }
        }

        HitStopComponent->StartMultipleHitStop(
            HitStopTargets,
            ThirdComboHitStopDuration,
            ThirdComboHitStopTimeScale);
    }
    
    if (bEnableHitKnockback && HitKnockbackStrength > 0.f)
    {
        if (ACharacter* HitCharacter = Cast<ACharacter>(HitActor))
        {
            if (HitCharacter->FindComponentByClass<UCEnemyHealthComponent>())
            {
                FVector KnockDirection = FVector::ZeroVector;
            
                // 적 캡슐 컴포넌트 중심 위치 가져오기
                UCapsuleComponent* HitCapsule = HitCharacter->GetCapsuleComponent();
                if (HitCapsule && OwnerChar.IsValid())
                {
                    // 플레이어 → 적 캡슐 중심 방향
                    FVector PlayerLoc = OwnerChar->GetActorLocation();
                    FVector EnemyLoc = HitCapsule->GetComponentLocation();
                
                    KnockDirection = (EnemyLoc - PlayerLoc);
                    KnockDirection.Z = 0.f;  // 수평 방향만
                    KnockDirection.Normalize();
                }
            
                // 예외 처리: 방향 계산 실패 시 폴백
                if (KnockDirection.IsNearlyZero())
                {
                    KnockDirection = HitCharacter->GetActorForwardVector();
                    KnockDirection.Z = 0.f;
                }
            
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
    bHitStopTriggeredThisCombo = false;
    
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

    if (ACPlayerCharacter* Player = Cast<ACPlayerCharacter>(GetOwner()))
    {
        if (UCPlayerEffectComponent* EffectComp = Player->GetEffectComponent())
        {
            EffectComp->PlayComboAttackEffect(CurrentCombo);
        }
    }

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