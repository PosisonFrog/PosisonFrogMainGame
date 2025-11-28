#include "CSkill_SpinAttack.h"
#include "Global.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraShakeBase.h"
#include "TimerManager.h"
#include "00_Character/00_Player/CPlayerCharacter.h"
#include "00_Character/00_Player/02_Weapon/CHammer.h"
#include "00_Character/02_Component/00_PlayerComponent/CFuryGaugeComponent.h"
#include "00_Character/02_Component/00_PlayerComponent/CPlayerWeaponComponent.h"
#include "99_Util/CLog.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/DamageType.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "00_Character/00_Player/CHitStopSubsystem.h"
#include "00_Character/02_Component/CHitStopComponent.h"
#include "00_Character/02_Component/00_PlayerComponent/CUltimateBuffComponent.h"
#include "00_Character/02_Component/00_PlayerComponent/ComboStackComponent.h"
#include "Engine/OverlapResult.h"


UCSkill_SpinAttack::UCSkill_SpinAttack()
{
    PrimaryComponentTick.bCanEverTick = false;

    OwnerChar = Cast<ACharacter>(GetOwner());
    if (!OwnerChar.IsValid())
    {
        CLog::Log(TEXT("[UCSkill_SpinAttack] OwnerCharacter invalid"));
        return;
    }
    
    if (!DamageTypeClass)
        DamageTypeClass = UDamageType::StaticClass();
    if (!FinisherDamageTypeClass)
        FinisherDamageTypeClass = UDamageType::StaticClass();
}

void UCSkill_SpinAttack::TryStartSpin()
{
    if (!FuryRef)
        return;
    
    StacksAtActivation = FuryRef->CurrentStacks;
    
    // Fury 스택이 가득 차 있을 때만 발동
    if (StacksAtActivation >= FuryRef->MaxStacks)
    {
        ActivateSkill();
        FuryRef->ActivateEffect();
    }
}

void UCSkill_SpinAttack::StopSpin()
{
    CancelSkill();
    
    if (FuryRef)
    {
        FuryRef->CancelEffect();
    }
}

void UCSkill_SpinAttack::StopAllEffects()
{
    StopSpinEffect();
}

bool UCSkill_SpinAttack::DoActivate()
{
    if (TimerHandle_SpinTick.IsValid())
        return false; // 이미 동작 중

    // 발동 시점 Fury 스냅샷(원하시면 OnFuryStarted/Ended에서 동적 갱신)
    bFuryActiveSnapshot = (FuryRef && FuryRef->IsEffectActive());

    // 히트스톱 시간 초기화
    LastHitStopTime = 0.f;

    if (ACPlayerCharacter* PlayerChar = Cast<ACPlayerCharacter>(OwnerChar))
    {
        PlayerChar->StartSpinSound();
        
        if (CharSpinMontage)
            PlayerChar->PlayAnimMontage(CharSpinMontage);

        ACHammer* Hammer = nullptr;
        if (UCPlayerWeaponComponent* WeaponComp = PlayerChar->FindComponentByClass<UCPlayerWeaponComponent>())
            Hammer = WeaponComp->GetHammer();

        if (Hammer && HammerSpinMontage)
        {
            if (UAnimInstance* HammerAnim = Hammer->GetHammerMesh()->GetAnimInstance())
                HammerAnim->Montage_Play(HammerSpinMontage);
        }

        // 나중에 만약 EffectComponent를 사용하게 된다면 주석 풀기
        /*if (ACPlayerCharacter* Player = Cast<ACPlayerCharacter>(OwnerChar.Get()))
        {
            if (UCPlayerEffectComponent* EffectComp = Player->GetEffectComponent())
            {
                EffectComp->PlaySpinAttackEffect();
            }
        }*/
        StartSpinEffect();
    }
    
    LastTickTime = GetWorld()->GetTimeSeconds();

    GetWorld()->GetTimerManager().SetTimer(
        TimerHandle_SpinTick,
        this, &UCSkill_SpinAttack::SpinTick,
        TickInterval, true);

    SpinTick(); // 즉시 1회
    return true;
}

bool UCSkill_SpinAttack::DoCancel()
{
    if (!TimerHandle_SpinTick.IsValid())
        return false;

    GetWorld()->GetTimerManager().ClearTimer(TimerHandle_SpinTick);

    StopSpinEffect();
    
    // 애니메이션 중지
    if (OwnerChar.IsValid())
    {
        if (ACPlayerCharacter* PlayerChar = Cast<ACPlayerCharacter>(OwnerChar))
        {
            PlayerChar->StopSpinSound();
            
            if (UAnimInstance* CharAnimInst = PlayerChar->GetMesh()->GetAnimInstance())
            {
                if (CharAnimInst->Montage_IsPlaying(CharSpinMontage))
                    CharAnimInst->Montage_Stop(0.2f, CharSpinMontage);
            }

            ACHammer* Hammer = nullptr;
            if (UCPlayerWeaponComponent* WeaponComp = PlayerChar->FindComponentByClass<UCPlayerWeaponComponent>())
                Hammer = WeaponComp->GetHammer();

            if (Hammer && HammerSpinMontage)
            {
                if (UAnimInstance* HammerAnim = Hammer->GetHammerMesh()->GetAnimInstance())
                    HammerAnim->Montage_Stop(0.2f, HammerSpinMontage);
            }

            // 나중에 만약 EffectComponent를 사용하게 된다면 주석 풀기
            // 새로운 이펙트 시스템을 통해 모든 활성 이펙트 정리
            /*if (UCPlayerEffectComponent* EffectComp = PlayerChar->GetEffectComponent())
            {
                EffectComp->StopAllActiveEffects();
            }*/
        }
    }
    
    return true;
}

void UCSkill_SpinAttack::OnFuryStarted(int32, float, float, int32)
{
    // 스킬 동작 중 Fury가 켜지면 이후 틱에 반영하고 싶을 때:
    // bFuryActiveSnapshot = true;
}

void UCSkill_SpinAttack::OnFuryEnded(bool /*bCanceled*/, float /*Remain*/)
{
    CancelSkill();
}

void UCSkill_SpinAttack::OnFuryFinisher(float FinisherDamage)
{
    CancelSkill();
    // 피니시 시작 전에 스핀 상태 완전히 정리
    if (TimerHandle_SpinTick.IsValid())
        GetWorld()->GetTimerManager().ClearTimer(TimerHandle_SpinTick);
    

    // 나중에 만약 EffectComponent를 사용하게 된다면 주석 풀기
    // 새로운 이펙트 시스템을 통해 모든 활성 이펙트 정리
    /*if (OwnerChar.IsValid())
    {
        if (ACPlayerCharacter* PlayerChar = Cast<ACPlayerCharacter>(OwnerChar))
        {
            if (UCPlayerEffectComponent* EffectComp = PlayerChar->GetEffectComponent())
            {
                EffectComp->StopAllActiveEffects();
            }
        }
    }*/
    StopSpinEffect();
    
    // Fury 10칸 피니시 발생 → ‘망치 내려찍기’ 연출
    PendingFinisherDamage = (FinisherDamage > 0.f) ? FinisherDamage : FinisherDamageDefault;
    PlayFinisherMontageAndScheduleImpact(PendingFinisherDamage);
}

void UCSkill_SpinAttack::SpinTick()
{
    if (!GetWorld()) return;

    const float Now = GetWorld()->GetTimeSeconds();
    const float Delta = FMath::Max(0.f, Now - LastTickTime);
    LastTickTime = Now;

    AActor* Owner = GetOwner();
    if (!Owner) return;

    // 1) 자동 회전
    if (bAutoRotateOwner && SpinYawSpeedDegPerSec > 0.f)
    {
        const float DeltaYaw = SpinYawSpeedDegPerSec * Delta;
        FRotator R = Owner->GetActorRotation();
        R.Yaw = FMath::UnwindDegrees(R.Yaw + DeltaYaw);
        Owner->SetActorRotation(R);
    }

    // 2) 반경 내 대상 수집
    TArray<AActor*> Targets;
    CollectTargetsInRadius(Targets, AttackRadius);

    // 3) 틱 피해 적용
    float DPS = BaseDPS;
    if (bFuryActiveSnapshot) DPS *= FuryDPSMultiplier;
    const float DamageThisTick = DPS * Delta;

    for (AActor* T : Targets)
    {
        ApplyDamageTo(T, DamageThisTick, DamageTypeClass);
    }

    if (Targets.Num() > 0)
    {
        if (ACPlayerCharacter* PlayerChar = Cast<ACPlayerCharacter>(OwnerChar.Get()))
        {
            if (UComboStackComponent* ComboComp = PlayerChar->GetComboStackComponent())
            {
                ComboComp->OnDirectHit(TEXT("Spin_Attack"), Now);
            }
        }
    }

    ApplySpinTickHitStop(Targets);
}

void UCSkill_SpinAttack::CollectTargetsInRadius(TArray<AActor*>& OutTargets, float Radius) const
{
    OutTargets.Reset();

    UWorld* World = GetWorld();
    AActor* Owner = GetOwner();
    if (!World || !Owner) return;

    const FVector Center = Owner->GetActorLocation();

    // Pawn 채널 오버랩
    FCollisionObjectQueryParams ObjParams;
    ObjParams.AddObjectTypesToQuery(ECC_Pawn);
    ObjParams.AddObjectTypesToQuery(PF::Collision::RiotEnemy);
    ObjParams.AddObjectTypesToQuery(PF::Collision::EnemyBody); 
    ObjParams.AddObjectTypesToQuery(PF::Collision::Projectile); 
    ObjParams.AddObjectTypesToQuery(PF::Collision::BossCharacter); 
    
    FCollisionShape Sphere = FCollisionShape::MakeSphere(Radius);
    FCollisionQueryParams  QueryParams(SCENE_QUERY_STAT(SpinAttackOverlap), false, Owner);

    TArray<FOverlapResult> Overlaps;
    const bool bAny = World->OverlapMultiByObjectType(
        Overlaps, Center, FQuat::Identity, ObjParams, Sphere, QueryParams);

    if (!bAny) return;

    for (const FOverlapResult& O : Overlaps)
    {
        AActor* HitActor = O.GetActor();
        if (!HitActor || HitActor == Owner) continue;

        // 높이 필터
        const float ZDiff = FMath::Abs(HitActor->GetActorLocation().Z - Center.Z);

        if (ZDiff > ZTolerance) continue;
        
        // TODO: 팀/태그/클래스(ACEnemyCharacterBase 등) 필터링 추가 권장
        OutTargets.Add(HitActor);
    }
}

void UCSkill_SpinAttack::ApplyDamageTo(AActor* Target, float DamageAmount, TSubclassOf<UDamageType> InDamageType) const
{
    
    if (!Target || DamageAmount <= 0.f) return;

    AActor* Owner = GetOwner();
    AController* InstController = nullptr;
    if (APawn* Pawn = Cast<APawn>(Owner))
        InstController = Pawn->GetController();

    // 일반 데미지 타입 -> 스택 증가 없음
    if (!InDamageType)
        InDamageType = UDamageType::StaticClass();


    if (ACPlayerCharacter* PlayerChar = Cast<ACPlayerCharacter>(OwnerChar))
    {
        PlayerChar->PlayAttackHitSound();
    }
    
    UGameplayStatics::ApplyDamage(
        Target,
        DamageAmount,
        InstController,
        Owner,
        InDamageType);
}

void UCSkill_SpinAttack::PlayFinisherMontageAndScheduleImpact(float FinisherDamage)
{
    float ImpactDelay = FinisherImpactDelay;

    if (ACPlayerCharacter* PlayerChar = Cast<ACPlayerCharacter>(OwnerChar))
    {
        // ※ 애님 노티파이로 정확 타이밍을 주면 더 좋지만(완전 C++라면),
        //   여기서는 간단히 FinisherImpactDelay 초 후에 충격 판정.
        
        if (CharFinisherMontage)
        {
            if (UAnimInstance* Anim = OwnerChar->GetMesh()->GetAnimInstance())
                Anim->Montage_Play(CharFinisherMontage);
        }

        ACHammer* Hammer = nullptr;
        if (UCPlayerWeaponComponent* WeaponComp = PlayerChar->FindComponentByClass<UCPlayerWeaponComponent>())
            Hammer = WeaponComp->GetHammer();

        if (Hammer && HammerFinisherMontage)
        {
            if (UAnimInstance* HammerAnim = Hammer->GetHammerMesh()->GetAnimInstance())
                HammerAnim->Montage_Play(HammerFinisherMontage);
        }
    }

    // 타이머로 충격 판정 예약
    FTimerHandle TempHandle;
    FTimerDelegate TimerDel;
    TimerDel.BindLambda([this]()
    {
        DoFinisherImpact();
    });
    GetWorld()->GetTimerManager().SetTimer(TempHandle, TimerDel, ImpactDelay, false);
}

void UCSkill_SpinAttack::DoFinisherImpact()
{
    if (!GetWorld()) return;

    AActor* Owner = GetOwner();
    if (!Owner) return;

    // 대상 수집(피니시는 별도 반경)
    TArray<AActor*> Targets;
    CollectTargetsInRadius(Targets, FinisherRadius);

    AController* Inst = nullptr;
    if (APawn* P = Cast<APawn>(Owner)) Inst = P->GetController();
    
    // 피해 적용
    if (!FinisherDamageTypeClass)
        FinisherDamageTypeClass = UDamageType::StaticClass();

    if (ACPlayerCharacter* PC = Cast<ACPlayerCharacter>(OwnerChar))
    {
        PC->PlayAttackHitSound();
    }

    
    for (AActor* T : Targets)
    {
        UGameplayStatics::ApplyDamage(
            T,
            PendingFinisherDamage,
            Inst,
            Owner,
            FinisherDamageTypeClass);
    }

    if (Targets.Num() > 0)
    {
        if (ACPlayerCharacter* PlayerChar = Cast<ACPlayerCharacter>(OwnerChar.Get()))
        {
            if (UComboStackComponent* ComboComp = PlayerChar->GetComboStackComponent())
            {
                const float Now = GetWorld()->GetTimeSeconds();
                ComboComp->OnDirectHit(TEXT("Spin_Finisher"), Now);
            }
        }
    }
    
    ApplyFinisherHitStop(Targets);

    

    // 피니셔 넉백: 플레이어 정면 방향으로 적들을 Launch (보스 판별하여 다른 강도 적용)
    if (bEnableFinisherKnockback && Targets.Num() > 0)
    {
        FVector LaunchDirection = Owner->GetActorForwardVector();
        LaunchDirection.Z = 0.f;
        LaunchDirection.Normalize();

        if (!LaunchDirection.IsNearlyZero())
        {
            FTimerHandle KnockbackTimer;
            FTimerDelegate KnockbackDelegate;
            KnockbackDelegate.BindLambda([this, Targets, LaunchDirection]()
            {
                for (AActor* Target : Targets)
                {
                    if (ACharacter* HitCharacter = Cast<ACharacter>(Target))
                    {
                        // 보스인지 확인
                        bool bIsBoss = HitCharacter->ActorHasTag(FName("Boss"));

                        // 보스면 보스용 넉백 값 사용, 아니면 일반 넉백 값 사용
                        float KnockbackStrength = bIsBoss ? FinisherBossKnockbackStrength : FinisherKnockbackStrength;
                        float KnockbackUpStrength = bIsBoss ? FinisherBossKnockbackUpStrength : FinisherKnockbackUpStrength;

                        if (KnockbackStrength > 0.f)
                        {
                            FVector LaunchVelocity = LaunchDirection * KnockbackStrength;
                            if (KnockbackUpStrength > 0.f)
                            {
                                LaunchVelocity.Z += KnockbackUpStrength;
                            }
                            
                            HitCharacter->LaunchCharacter(LaunchVelocity, true, KnockbackUpStrength > 0.f);
                        }
                    }
                }
            });
            
            GetWorld()->GetTimerManager().SetTimer(
                KnockbackTimer,
                KnockbackDelegate,
                FinisherPlayerHitStopDuration,
                false
            );
        }
    }
    
    // 연출(선택)
    if (FinisherImpactSFX)
        UGameplayStatics::PlaySoundAtLocation(GetWorld(), FinisherImpactSFX, Owner->GetActorLocation());
    if (FinisherCameraShake)
        if (APlayerController* PC = Cast<APlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0)))
            PC->ClientStartCameraShake(FinisherCameraShake);
}

void UCSkill_SpinAttack::StartSpinEffect()
{
    if (!OwnerChar.IsValid())
        return;

    // 기존 이펙트가 있다면 먼저 정리
    StopSpinEffect();

    // 궁극기 상태 확인
    bool bIsUltimateActive = false;
    if (ACPlayerCharacter* PlayerChar = Cast<ACPlayerCharacter>(OwnerChar))
    {
        if (UCUltimateBuffComponent* UltComp = PlayerChar->FindComponentByClass<UCUltimateBuffComponent>())
        {
            bIsUltimateActive = UltComp->IsUltActive();
        }
    }

    // 궁극기 상태에 따라 적절한 이펙트 선택
    UNiagaraSystem* SelectedVFX = bIsUltimateActive ? SpinVFX_Ultimate : SpinVFX_Normal;

    if (SelectedVFX)
    {
        // 플레이어의 Root에 Attach
        ActiveSpinVFXComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
            SelectedVFX,
            OwnerChar->GetRootComponent(),
            NAME_None,
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            EAttachLocation::KeepRelativeOffset,
            true
        );

        if (ActiveSpinVFXComponent)
        {
            CLog::Log(FString::Printf(TEXT("스핀 이펙트 시작 (궁극기: %s)"), bIsUltimateActive ? TEXT("Yes") : TEXT("No")));
        }
    }
}

void UCSkill_SpinAttack::StopSpinEffect()
{
    if (ActiveSpinVFXComponent && ActiveSpinVFXComponent->IsActive())
    {
        ActiveSpinVFXComponent->Deactivate();
        ActiveSpinVFXComponent->DestroyComponent();
        ActiveSpinVFXComponent = nullptr;
        
        CLog::Log(TEXT("스핀 이펙트 중지"));
    }
}

void UCSkill_SpinAttack::ApplySpinTickHitStop(const TArray<AActor*>& Targets)
{
    if (!bEnableSpinTickHitStop || Targets.Num() == 0)
        return;

    AActor* Owner = GetOwner();
    if (!Owner)
        return;

    const float Now = GetWorld()->GetTimeSeconds();
    const float TimeSinceLastHitStop = Now - LastHitStopTime;
    
    if (TimeSinceLastHitStop < SpinHitStopInterval)
        return;

    LastHitStopTime = Now;

    if (UGameInstance* GameInst = GetWorld()->GetGameInstance())
    {
        if (UCHitStopSubsystem* HitStopSys = GameInst->GetSubsystem<UCHitStopSubsystem>())
        {
            HitStopSys->StartHitStop(
                Owner,
                SpinTickPlayerHitStopDuration,
                SpinTickPlayerHitStopTimeScale
            );

            // 보스가 아닌 적들에게만 히트스톱 적용
            for (AActor* T : Targets)
            {
                bool bIsBoss = T->ActorHasTag(FName("Boss"));
                if (!bIsBoss)
                {
                    HitStopSys->StartHitStop(
                        T,
                        SpinTickEnemyHitStopDuration,
                        SpinTickEnemyHitStopTimeScale
                    );
                }
            }
        }
    }
}

void UCSkill_SpinAttack::ApplyFinisherHitStop(const TArray<AActor*>& Targets)
{
    if (!bEnableHitStop || Targets.Num() == 0)
        return;

    AActor* Owner = GetOwner();
    if (!Owner)
        return;

    if (UGameInstance* GameInst = GetWorld()->GetGameInstance())
    {
        if (UCHitStopSubsystem* HitStopSys = GameInst->GetSubsystem<UCHitStopSubsystem>())
        {
            HitStopSys->StartHitStop(
                Owner,
                FinisherPlayerHitStopDuration,
                FinisherPlayerHitStopTimeScale
            );

            // 보스가 아닌 적들에게만 히트스톱 적용
            for (AActor* T : Targets)
            {
                bool bIsBoss = T->ActorHasTag(FName("Boss"));
                if (!bIsBoss)
                {
                    HitStopSys->StartHitStop(
                        T,
                        FinisherEnemyHitStopDuration,
                        FinisherEnemyHitStopTimeScale
                    );
                }
            }
        }
    }
}
