#include "00_Character/02_Component/00_PlayerComponent/CPlayerWeaponComponent.h"

#include "CPlayerEffectComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "00_Character/00_Player/CPlayerCharacter.h"
#include "00_Character/00_Player/02_Weapon/CHammer.h"
#include "00_Character/02_Component/00_PlayerComponent/Buffable.h"
#include "00_Character/02_Component/01_EnemyComponent/CEnemyHealthComponent.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "TimerManager.h"
#include "00_Character/00_Player/CHitStopSubsystem.h"
#include "00_Character/02_Component/CHitStopComponent.h"
#include "Engine/World.h"
#include "99_Util/CLog.h"
#include "Components/CapsuleComponent.h"


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

    CurrentHammer = Cast<ACHammer>(CurrentWeapon);
    if (!IsValid(CurrentHammer))
    {
        CLog::Log(TEXT("[WeaponComp] CurrentWeapon is not ACHammer"));
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


    if (!bHitSoundPlayedThisCombo)
    {
        if (ACPlayerCharacter* PC = Cast<ACPlayerCharacter>(OwnerChar.Get()))
        {
            PC->PlayAttackHitSound();
            bHitSoundPlayedThisCombo = true; // [추가] 재생 완료 표시 (이후 타격부터는 소리 안 남)
        }
    }

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

    SpawnHitEffect(HitActor, Hit);

    float PlayerDuration = 0.f;
    float PlayerTimeScale = 0.f;
    float EnemyDuration = 0.f;
    float EnemyTimeScale = 0.f;
    // 콤보 인덱스에 따라 히트스톱 설정 선택
    if (GetComboHitStopParams(CurrentCombo, PlayerDuration, PlayerTimeScale, EnemyDuration, EnemyTimeScale))
    {
        ApplyComboHitStop(HitActor, PlayerDuration, PlayerTimeScale, EnemyDuration, EnemyTimeScale);
    }
    
    if (bEnableHitKnockback)
    {
        // 보스인지 확인 (Boss 태그로 확인)
        bool bIsBoss = HitActor->ActorHasTag(FName("Boss"));

        // 보스면 보스용 넉백 값 사용, 아니면 일반 넉백 값 사용
        float CurrentKnockbackStrength = 0.f;
        float CurrentKnockbackUpStrength = 0.f;

        // 배열 범위 체크 - 범위를 벗어나면 마지막 값 사용
        const int32 KnockbackIndex = FMath::Min(CurrentCombo, HitKnockbackStrengths.Num() - 1);

        if (bIsBoss)
        {
            CurrentKnockbackStrength = BossKnockbackStrengths.IsValidIndex(KnockbackIndex) 
                ? BossKnockbackStrengths[KnockbackIndex] 
                : 250.f; // 보스 기본값
        
            CurrentKnockbackUpStrength = BossKnockbackUpStrengths.IsValidIndex(KnockbackIndex)
                ? BossKnockbackUpStrengths[KnockbackIndex]
                : 60.f; // 보스 기본값
        }
        else
        {
            CurrentKnockbackStrength = HitKnockbackStrengths.IsValidIndex(KnockbackIndex) 
                ? HitKnockbackStrengths[KnockbackIndex] 
                : 650.f; // 일반 적 기본값
        
            CurrentKnockbackUpStrength = HitKnockbackUpStrengths.IsValidIndex(KnockbackIndex)
                ? HitKnockbackUpStrengths[KnockbackIndex]
                : 120.f; // 일반 적 기본값
        }
        
        // 넉백 강도 체크 수정
        if (CurrentKnockbackStrength > 0.f)
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
                        // 콤보별 넉백 강도 사용
                        FVector LaunchVelocity = KnockDirection * CurrentKnockbackStrength;
                        if (CurrentKnockbackUpStrength > 0.f)
                        {
                            LaunchVelocity.Z += CurrentKnockbackUpStrength;
                        }
                    
                        HitCharacter->LaunchCharacter(LaunchVelocity, true, CurrentKnockbackUpStrength > 0.f);
                    }
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
    bHitSoundPlayedThisCombo = false;
    
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

    if (ACPlayerCharacter* PC = Cast<ACPlayerCharacter>(OwnerChar.Get()))
    {
        PC->PlayWeaponSwingSound();
    }

    // 나중에 만약 EffectComponent를 사용하게 된다면 주석 풀기
    /*if (ACPlayerCharacter* Player = Cast<ACPlayerCharacter>(GetOwner()))
    {
        if (UCPlayerEffectComponent* EffectComp = Player->GetEffectComponent())
        {
            EffectComp->PlayComboAttackEffect(CurrentCombo);
        }
    }*/

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

void UCPlayerWeaponComponent::SpawnHitEffect(AActor* HitActor, const FHitResult& HitInfo)
{
    if (!HitActor)
        return;

    ACHammer* Hammer = GetHammer();
    if (!Hammer)
    {
        CLog::Log(TEXT("[HitEffect] Hammer is null"));
        return;
    }

    bool bIsUltimateActive = CheckUltimateActive();
    UNiagaraSystem* SelectedEffect = bIsUltimateActive ? Hammer->GetHitEffect_Ultimate() : Hammer->GetHitEffect_Normal();

    if (!SelectedEffect)
    {
        CLog::Log(FString::Printf(TEXT("[HitEffect] Selected effect is NULL (Ultimate: %s)"), 
            bIsUltimateActive ? TEXT("Yes") : TEXT("No")));
        return;
    }

    FVector LocationOffset = Hammer->GetHitEffectLocationOffset();
    FRotator RotationOffset = Hammer->GetHitEffectRotationOffset();
    float EffectScale = Hammer->GetHitEffectScale();

    FTransform EffectTransform = CalculateEffectTransform(HitInfo, LocationOffset, RotationOffset);

    UNiagaraFunctionLibrary::SpawnSystemAtLocation(
        GetWorld(),
        SelectedEffect,
        EffectTransform.GetLocation(),
        EffectTransform.Rotator(),
        FVector(EffectScale),
        true,
        true
    );
}

bool UCPlayerWeaponComponent::CheckUltimateActive() const
{
    if (!OwnerChar.IsValid())
    {
        CLog::Log(TEXT("[CheckUlt] OwnerChar is invalid"));
        return false;
    }

    if (ACPlayerCharacter* PlayerChar = Cast<ACPlayerCharacter>(OwnerChar.Get()))
    {
        bool bUltActive = PlayerChar->IsUltimateActive();
        return bUltActive;
    }

    return false;
}

FTransform UCPlayerWeaponComponent::CalculateEffectTransform(const FHitResult& HitInfo, const FVector& LocationOffset, const FRotator& RotationOffset) const
{
    FVector SpawnLocation = HitInfo.ImpactPoint;
    
    if (!LocationOffset.IsZero() && !HitInfo.ImpactNormal.IsZero())
    {
        SpawnLocation += HitInfo.ImpactNormal * LocationOffset.Z;
    }
    else if (!LocationOffset.IsZero())
    {
        SpawnLocation += LocationOffset;
    }

    FRotator SpawnRotation = HitInfo.ImpactNormal.Rotation() + RotationOffset;

    return FTransform(SpawnRotation, SpawnLocation);
}

bool UCPlayerWeaponComponent::GetComboHitStopParams(int32 ComboIndex, float& OutPlayerDuration,
    float& OutPlayerTimeScale, float& OutEnemyDuration, float& OutEnemyTimeScale) const
{
    switch (ComboIndex)
    {
    case 0:
        if (!bEnableFirstComboHitStop) return false;
        OutPlayerDuration = FirstComboPlayerHitStopDuration;
        OutPlayerTimeScale = FirstComboPlayerHitStopTimeScale;
        OutEnemyDuration = FirstComboEnemyHitStopDuration;
        OutEnemyTimeScale = FirstComboEnemyHitStopTimeScale;
        return true;
        
    case 1:
        if (!bEnableSecondComboHitStop) return false;
        OutPlayerDuration = SecondComboPlayerHitStopDuration;
        OutPlayerTimeScale = SecondComboPlayerHitStopTimeScale;
        OutEnemyDuration = SecondComboEnemyHitStopDuration;
        OutEnemyTimeScale = SecondComboEnemyHitStopTimeScale;
        return true;
        
    case 2:
        if (!bEnableThirdComboHitStop) return false;
        OutPlayerDuration = ThirdComboPlayerHitStopDuration;
        OutPlayerTimeScale = ThirdComboPlayerHitStopTimeScale;
        OutEnemyDuration = ThirdComboEnemyHitStopDuration;
        OutEnemyTimeScale = ThirdComboEnemyHitStopTimeScale;
        return true;
        
    default:
        return false;
    }
}

void UCPlayerWeaponComponent::ApplyComboHitStop(AActor* HitActor, float PlayerDuration, float PlayerTimeScale,
    float EnemyDuration, float EnemyTimeScale)
{
    if (!bHitStopTriggeredThisCombo)
    {
        bHitStopTriggeredThisCombo = true;
        
        if (UGameInstance* GameInst = GetWorld()->GetGameInstance())
        {
            if (UCHitStopSubsystem* HitStopSys = GameInst->GetSubsystem<UCHitStopSubsystem>())
            {
                // 플레이어 애니메이션 정지
                if (OwnerChar.IsValid() && OwnerChar->GetMesh())
                {
                    if (UAnimInstance* PlayerAnimInst = OwnerChar->GetMesh()->GetAnimInstance())
                    {
                        PauseAndScheduleResumeAnimation(PlayerAnimInst, PlayerDuration);
                    }
                }

                // 해머 애니메이션 정지
                ACHammer* Hammer = GetHammer();
                if (IsValid(Hammer) && Hammer->GetHammerMesh())
                {
                    if (UAnimInstance* HammerAnimInst = Hammer->GetHammerMesh()->GetAnimInstance())
                    {
                        PauseAndScheduleResumeAnimation(HammerAnimInst, PlayerDuration);
                    }
                }

                // 히트스톱 적용
                HitStopSys->StartPlayerAndEnemyHitStop(
                    OwnerChar.Get(), HitActor,
                    PlayerDuration, PlayerTimeScale,
                    EnemyDuration, EnemyTimeScale
                );

                // 해머도 플레이어와 동일한 히트스톱
                if (IsValid(CurrentWeapon))
                {
                    HitStopSys->StartHitStop(CurrentWeapon, PlayerDuration, PlayerTimeScale);
                }
            }
        }
    }
}

void UCPlayerWeaponComponent::PauseAndScheduleResumeAnimation(UAnimInstance* AnimInst, float ResumeDelay)
{
    if (!AnimInst) return;
    
    if (UAnimMontage* CurrentMontage = AnimInst->GetCurrentActiveMontage())
    {
        AnimInst->Montage_Pause(CurrentMontage);
        
        FTimerHandle ResumeTimer;
        GetWorld()->GetTimerManager().SetTimer(
            ResumeTimer,
            [AnimInst, CurrentMontage]()
            {
                if (IsValid(AnimInst) && IsValid(CurrentMontage))
                {
                    AnimInst->Montage_Resume(CurrentMontage);
                }
            },
            ResumeDelay,
            false
        );
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