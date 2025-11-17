#include "CPlayerEffectComponent.h"
#include "00_Character/00_Player/CPlayerCharacter.h"
#include "00_Character/00_Player/02_Weapon/CHammer.h"
#include "00_Character/02_Component/00_PlayerComponent/CPlayerWeaponComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "99_Util/CLog.h"

UCPlayerEffectComponent::UCPlayerEffectComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    // 기본 콤보 공격 이펙트 3개 초기화
    ComboAttackEffects.SetNum(3);
    
    // 커맨드 이펙트 배열 초기화 (먼지, 크랙, 무기 스윙)
    CommandLaunchEffects.SetNum(3);
    CommandSlamEffects.SetNum(3);

    // Timer 배열 초기화
    ComboAttackTimers.SetNum(3);
    CommandLaunchTimers.SetNum(3);
    CommandSlamTimers.SetNum(3);
}

void UCPlayerEffectComponent::BeginPlay()
{
    Super::BeginPlay();

    // 플레이어 캐릭터 캐시
    OwnerPlayer = Cast<ACPlayerCharacter>(GetOwner());
    
    if (!OwnerPlayer.IsValid())
        CLog::Log(TEXT("CPlayerEffectComponent: Owner is not ACPlayerCharacter!"));
}

void UCPlayerEffectComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 모든 Timer 정리
    ClearAllEffectTimers();

    // 활성 이펙트 컴포넌트 정리
    for (UNiagaraComponent* Effect : ActiveEffects)
    {
        if (Effect && IsValid(Effect))
            Effect->DestroyComponent();
    }
    ActiveEffects.Empty();

    // 궁극기 무기 이펙트 정리
    if (ActiveUltimateWeaponEffect && IsValid(ActiveUltimateWeaponEffect))
    {
        ActiveUltimateWeaponEffect->DestroyComponent();
        ActiveUltimateWeaponEffect = nullptr;
    }

    Super::EndPlay(EndPlayReason);
}

// ────────────────────────────────────────────────────────────────────────────
// 이펙트 재생 함수
// ────────────────────────────────────────────────────────────────────────────
void UCPlayerEffectComponent::PlayComboAttackEffect(int32 ComboIndex)
{
    if (!ComboAttackEffects.IsValidIndex(ComboIndex))
    {
        CLog::Log(FString::Printf(TEXT("CPlayerEffectComponent: Invalid combo index %d"), ComboIndex));
        return;
    }

    // 이전 Timer 정리
    if (ComboAttackTimers.IsValidIndex(ComboIndex))
        ClearEffectTimer(ComboAttackTimers[ComboIndex]);

    const FEffectStatePair& EffectPair = ComboAttackEffects[ComboIndex];
    const FEffectSpawnSettings& Settings = GetCurrentStateEffect(EffectPair);

    // 일반 공격은 Hammer의 Joy_WeaponVFX 소켓 기준
    ACHammer* Hammer = GetHammer();
    if (!Hammer)
    {
        CLog::Log(TEXT("CPlayerEffectComponent: Cannot play combo attack effect - Hammer not found"));
        return;
    }

    // TimerHandle 전달하여 Delay 처리
    FTimerHandle* TimerHandle = ComboAttackTimers.IsValidIndex(ComboIndex) ? &ComboAttackTimers[ComboIndex] : nullptr;
    SpawnEffect(Settings, Hammer, TimerHandle);
}

void UCPlayerEffectComponent::PlaySpinAttackEffect()
{
    ClearEffectTimer(SpinAttackTimer);

    const FEffectSpawnSettings& Settings = GetCurrentStateEffect(SpinAttackEffect);
    
    // 스핀 공격은 Hammer의 Joy_WeaponVFX 소켓 기준
    ACHammer* Hammer = GetHammer();
    if (!Hammer)
    {
        CLog::Log(TEXT("CPlayerEffectComponent: Cannot play spin attack effect - Hammer not found"));
        return;
    }

    UNiagaraComponent* SpawnedEffect = SpawnEffect(Settings, Hammer, &SpinAttackTimer);
    
    // 수동 관리가 필요한 경우 (즉시 스폰된 경우만)
    if (SpawnedEffect && !Settings.bAutoDestroy)
        ActiveEffects.Add(SpawnedEffect);
}

void UCPlayerEffectComponent::PlaySpinFinisherEffect()
{
    ClearEffectTimer(SpinFinisherTimer);

    const FEffectSpawnSettings& Settings = GetCurrentStateEffect(SpinFinisherEffect);
    
    // 스핀 피니셔는 Hammer의 Joy_WeaponVFX 소켓 기준
    ACHammer* Hammer = GetHammer();
    if (!Hammer)
    {
        CLog::Log(TEXT("CPlayerEffectComponent: Cannot play spin finisher effect - Hammer not found"));
        return;
    }

    SpawnEffect(Settings, Hammer, &SpinFinisherTimer);
}

void UCPlayerEffectComponent::PlayCommandLaunchEffect()
{
    // 여러 이펙트를 순차적으로 재생 (먼지, 크랙, 무기 스윙)
    for (int32 i = 0; i < CommandLaunchEffects.Num(); ++i)
    {
        if (CommandLaunchTimers.IsValidIndex(i))
        {
            ClearEffectTimer(CommandLaunchTimers[i]);
        }

        const FEffectStatePair& EffectPair = CommandLaunchEffects[i];
        const FEffectSpawnSettings& Settings = GetCurrentStateEffect(EffectPair);

        // 모든 커맨드 이펙트는 Hammer 기준
        ACHammer* Hammer = GetHammer();
        if (Hammer)
        {
            FTimerHandle* TimerHandle = CommandLaunchTimers.IsValidIndex(i) ? &CommandLaunchTimers[i] : nullptr;
            SpawnEffect(Settings, Hammer, TimerHandle);
        }
        else
        {
            CLog::Log(TEXT("CPlayerEffectComponent: Cannot play command launch effect - Hammer not found"));
        }
    }
}

void UCPlayerEffectComponent::PlayCommandSlamEffect()
{
    // 여러 이펙트를 순차적으로 재생
    for (int32 i = 0; i < CommandSlamEffects.Num(); ++i)
    {
        if (CommandSlamTimers.IsValidIndex(i))
        {
            ClearEffectTimer(CommandSlamTimers[i]);
        }

        const FEffectStatePair& EffectPair = CommandSlamEffects[i];
        const FEffectSpawnSettings& Settings = GetCurrentStateEffect(EffectPair);

        // 모든 커맨드 이펙트는 Hammer 기준
        ACHammer* Hammer = GetHammer();
        if (Hammer)
        {
            FTimerHandle* TimerHandle = CommandSlamTimers.IsValidIndex(i) ? &CommandSlamTimers[i] : nullptr;
            SpawnEffect(Settings, Hammer, TimerHandle);
        }
        else
        {
            CLog::Log(TEXT("CPlayerEffectComponent: Cannot play command slam effect - Hammer not found"));
        }
    }
}

void UCPlayerEffectComponent::PlayDashEffect()
{
    ClearEffectTimer(DashEffectTimer);

    const FEffectSpawnSettings& Settings = GetCurrentStateEffect(DashEffect);
    
    // 대쉬는 플레이어 캐릭터 기준
    ACPlayerCharacter* Player = GetPlayerCharacter();
    if (!Player)
    {
        return;
    }

    SpawnEffect(Settings, Player, &DashEffectTimer);
}

void UCPlayerEffectComponent::PlayUltimateActivationEffect()
{
    ClearEffectTimer(UltimateActivationTimer);

    // 궁극기 활성화 이펙트는 플레이어 캐릭터 기준
    ACPlayerCharacter* Player = GetPlayerCharacter();
    if (!Player)
    {
        return;
    }

    SpawnEffect(UltimateActivationEffect, Player, &UltimateActivationTimer);
}

UNiagaraComponent* UCPlayerEffectComponent::AttachUltimateWeaponEffect()
{
    // 기존 이펙트가 있으면 제거
    DetachUltimateWeaponEffect();

    // 궁극기 무기 이펙트는 Hammer의 Joy_WeaponVFX 소켓 기준
    ACHammer* Hammer = GetHammer();
    if (!Hammer)
    {
        CLog::Log(TEXT("CPlayerEffectComponent: Cannot attach ultimate weapon effect - Hammer not found"));
        return nullptr;
    }

    if (!UltimateWeaponEffect.EffectSystem)
    {
        CLog::Log(TEXT("CPlayerEffectComponent: Ultimate weapon effect system is null"));
        return nullptr;
    }

    // 궁극기 무기 이펙트는 즉시 부착 (Delay 무시)
    ActiveUltimateWeaponEffect = SpawnEffectImmediate(UltimateWeaponEffect, Hammer);
    
    return ActiveUltimateWeaponEffect;
}

void UCPlayerEffectComponent::DetachUltimateWeaponEffect()
{
    if (ActiveUltimateWeaponEffect && IsValid(ActiveUltimateWeaponEffect))
    {
        ActiveUltimateWeaponEffect->DestroyComponent();
        ActiveUltimateWeaponEffect = nullptr;
    }
}

// ────────────────────────────────────────────────────────────────────────────
// Timer 관리
// ────────────────────────────────────────────────────────────────────────────
void UCPlayerEffectComponent::ClearAllEffectTimers()
{
    UWorld* World = GetWorld();
    if (!World)
        return;

    // 콤보 공격 Timers
    for (FTimerHandle& Timer : ComboAttackTimers)
        ClearEffectTimer(Timer);

    // 스핀 Timers
    ClearEffectTimer(SpinAttackTimer);
    ClearEffectTimer(SpinFinisherTimer);

    // 커맨드 Timers
    for (FTimerHandle& Timer : CommandLaunchTimers)
        ClearEffectTimer(Timer);
    
    for (FTimerHandle& Timer : CommandSlamTimers)
        ClearEffectTimer(Timer);

    // 대쉬 Timer
    ClearEffectTimer(DashEffectTimer);

    // 궁극기 Timer
    ClearEffectTimer(UltimateActivationTimer);
}

void UCPlayerEffectComponent::ClearEffectTimer(FTimerHandle& TimerHandle)
{
    UWorld* World = GetWorld();
    if (!World)
        return;

    if (TimerHandle.IsValid())
    {
        World->GetTimerManager().ClearTimer(TimerHandle);
        TimerHandle.Invalidate();
    }
}

// ────────────────────────────────────────────────────────────────────────────
// 이펙트 중단
// ────────────────────────────────────────────────────────────────────────────
void UCPlayerEffectComponent::StopAllActiveEffects()
{
    for (UNiagaraComponent* Effect : ActiveEffects)
    {
        if (Effect && IsValid(Effect))
        {
            Effect->Deactivate();
            Effect->DestroyComponent();
        }
    }
    ActiveEffects.Empty();
}

void UCPlayerEffectComponent::StopActiveEffect(UNiagaraComponent* EffectComponent)
{
    if (!EffectComponent || !IsValid(EffectComponent))
        return;

    if (ActiveEffects.Contains(EffectComponent))
    {
        EffectComponent->Deactivate();
        EffectComponent->DestroyComponent();
        ActiveEffects.Remove(EffectComponent);
    }
}

// ────────────────────────────────────────────────────────────────────────────
// 내부 헬퍼 함수
// ────────────────────────────────────────────────────────────────────────────
const FEffectSpawnSettings& UCPlayerEffectComponent::GetCurrentStateEffect(const FEffectStatePair& EffectPair) const
{
    if (IsUltimateActive())
        return EffectPair.UltimateEffect;
    else
        return EffectPair.NormalEffect;
}

UNiagaraComponent* UCPlayerEffectComponent::SpawnEffect(const FEffectSpawnSettings& Settings, AActor* TargetActor, FTimerHandle* OutTimerHandle)
{
    if (!Settings.EffectSystem)
        return nullptr;

    if (!TargetActor)
    {
        CLog::Log(TEXT("CPlayerEffectComponent: Target actor is null for effect spawn"));
        return nullptr;
    }

    UWorld* World = GetWorld();
    if (!World)
        return nullptr;

    // Delay가 있는 경우 Timer로 지연 재생
    if (Settings.SpawnDelay > 0.0f)
    {
        if (OutTimerHandle)
        {
            ClearEffectTimer(*OutTimerHandle);
            
            World->GetTimerManager().SetTimer(
                *OutTimerHandle,
                [this, Settings, TargetActor]()
                {
                    UNiagaraComponent* DelayedEffect = SpawnEffectImmediate(Settings, TargetActor);
                    
                    // 수동 관리가 필요한 이펙트는 배열에 추가
                    if (DelayedEffect && !Settings.bAutoDestroy)
                    {
                        ActiveEffects.Add(DelayedEffect);
                    }
                },
                Settings.SpawnDelay,
                false
            );
        }
        else
        {
            CLog::Log(TEXT("CPlayerEffectComponent: Warning - Timer handle is null but delay is set. Effect will spawn immediately."));
            return SpawnEffectImmediate(Settings, TargetActor);
        }
        
        return nullptr; // Timer로 실행되므로 nullptr 반환
    }
    
    // Delay가 없는 경우 즉시 재생
    return SpawnEffectImmediate(Settings, TargetActor);
}

UNiagaraComponent* UCPlayerEffectComponent::SpawnEffectImmediate(const FEffectSpawnSettings& Settings, AActor* TargetActor)
{
    if (!Settings.EffectSystem || !TargetActor)
        return nullptr;

    UWorld* World = GetWorld();
    if (!World)
        return nullptr;

    USceneComponent* AttachComponent = nullptr;
    
    // 소켓 이름이 지정되어 있으면 해당 소켓 찾기
    if (Settings.SocketName != NAME_None)
    {
        if (ACharacter* Character = Cast<ACharacter>(TargetActor))
        {
            AttachComponent = Character->GetMesh();
        }
        else if (ACHammer* Hammer = Cast<ACHammer>(TargetActor))
        {
            AttachComponent = Hammer->GetHammerMesh();
        }
        else
        {
            AttachComponent = TargetActor->GetRootComponent();
        }
    }
    else
    {
        AttachComponent = TargetActor->GetRootComponent();
    }

    if (!AttachComponent)
    {
        CLog::Log(TEXT("CPlayerEffectComponent: Attach component not found"));
        return nullptr;
    }

    UNiagaraComponent* NiagaraComp = nullptr;

    if (Settings.bDetachFromSocket)
    {
        // 위치 고정 모드: 월드 공간에 스폰
        FVector WorldLocation;
        FRotator WorldRotation;

        if (Settings.SocketName != NAME_None && AttachComponent->DoesSocketExist(Settings.SocketName))
        {
            FTransform SocketTransform = AttachComponent->GetSocketTransform(Settings.SocketName);
            WorldLocation = SocketTransform.TransformPosition(Settings.LocationOffset);
            WorldRotation = (SocketTransform.GetRotation() * Settings.RotationOffset.Quaternion()).Rotator();
        }
        else
        {
            WorldLocation = AttachComponent->GetComponentLocation() + Settings.LocationOffset;
            WorldRotation = AttachComponent->GetComponentRotation() + Settings.RotationOffset;
        }

        NiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            World,
            Settings.EffectSystem,
            WorldLocation,
            WorldRotation,
            Settings.Scale,
            Settings.bAutoDestroy,
            true,
            ENCPoolMethod::None
        );
    }
    else
    {
        // 소켓 부착 모드
        NiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
            Settings.EffectSystem,
            AttachComponent,
            Settings.SocketName,
            Settings.LocationOffset,
            Settings.RotationOffset,
            Settings.Scale,
            EAttachLocation::KeepRelativeOffset,
            Settings.bAutoDestroy,
            ENCPoolMethod::None
        );
    }

    // 수동 관리가 필요한 이펙트는 배열에 추가
    if (NiagaraComp && !Settings.bAutoDestroy)
    {
        ActiveEffects.Add(NiagaraComp);
    }

    return NiagaraComp;
}

ACPlayerCharacter* UCPlayerEffectComponent::GetPlayerCharacter() const
{
    if (OwnerPlayer.IsValid())
        return OwnerPlayer.Get();
    
    return nullptr;
}

ACHammer* UCPlayerEffectComponent::GetHammer() const
{
    ACPlayerCharacter* Player = GetPlayerCharacter();
    if (!Player)
        return nullptr;

    UCPlayerWeaponComponent* WeaponComp = Player->FindComponentByClass<UCPlayerWeaponComponent>();
    if (!WeaponComp)
        return nullptr;

    return WeaponComp->GetHammer();
}

bool UCPlayerEffectComponent::IsUltimateActive() const
{
    ACPlayerCharacter* Player = GetPlayerCharacter();
    if (!Player)
    {
        return false;
    }

    // 플레이어의 궁극기 상태 확인
    // CPlayerCharacter에서 bUltActive 변수 참조
    return Player->IsBuffActive(); // IBuffable 인터페이스 활용
}