#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "CPlayerEffectComponent.generated.h"

class ACPlayerCharacter;
class ACHammer;

/**
 * 이펙트 스폰 설정 구조체
 * 에디터에서 소켓, 위치, 회전, 고정 설정, 타이밍 조절 가능
 */
USTRUCT(BlueprintType)
struct FEffectSpawnSettings
{
    GENERATED_BODY()

    // 이펙트 시스템
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
    TObjectPtr<UNiagaraSystem> EffectSystem = nullptr;

    // 이펙트가 스폰될 소켓 이름 (기본: Joy_WeaponVFX, None이면 액터 위치 사용)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
    FName SocketName = TEXT("Joy_WeaponVFX");

    // 소켓 기준 상대 위치 오프셋
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
    FVector LocationOffset = FVector::ZeroVector;

    // 소켓 기준 상대 회전 오프셋
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
    FRotator RotationOffset = FRotator::ZeroRotator;

    // true면 위치 고정, false면 소켓에 부착
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
    bool bDetachFromSocket = false;

    // 이펙트 스케일
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
    FVector Scale = FVector(1.0f);

    // 자동 소멸 여부 (false면 수동으로 관리)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
    bool bAutoDestroy = true;

    // 이펙트 재생 지연 시간 (초 단위)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect|Timing")
    float SpawnDelay = 0.0f;

    FEffectSpawnSettings()
        : EffectSystem(nullptr)
        , SocketName(TEXT("Joy_WeaponVFX"))  // 기본값: Hammer 소켓
        , LocationOffset(FVector::ZeroVector)
        , RotationOffset(FRotator::ZeroRotator)
        , bDetachFromSocket(false)
        , Scale(FVector(1.0f))
        , bAutoDestroy(true)
        , SpawnDelay(0.0f)  // 기본값: 즉시 재생
    {}
};

/**
 * 궁극기/일반 상태별 이펙트 세트
 */
USTRUCT(BlueprintType)
struct FEffectStatePair
{
    GENERATED_BODY()

    // 일반 상태 이펙트
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
    FEffectSpawnSettings NormalEffect;

    // 궁극기 상태 이펙트
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
    FEffectSpawnSettings UltimateEffect;
};

/**
 * 플레이어 이펙트 컴포넌트
 * - 궁극기 상태와 일반 상태에 따라 다른 이펙트 재생
 * - Timer로 이펙트 관리 및 타이밍 조절
 * - Pause/Restart/Death 시 Timer 정리
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class POSISONFROG_API UCPlayerEffectComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCPlayerEffectComponent();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    // ─────────── 이펙트 재생 함수 ───────────
    // 일반 공격 이펙트 재생 (콤보 인덱스: 0, 1, 2) - Hammer 기준
    void PlayComboAttackEffect(int32 ComboIndex);

    // 스핀 공격 이펙트 재생 - Hammer 기준
    void PlaySpinAttackEffect();

    // 스핀 피니셔 이펙트 재생 - Hammer 기준
    void PlaySpinFinisherEffect();

    // 커맨드 공격 이펙트 재생 (Launch/Slam) - Hammer 기준
    void PlayCommandLaunchEffect();
    void PlayCommandSlamEffect();

    // 대쉬 이펙트 재생 - Player 기준
    void PlayDashEffect();

    // 궁극기 활성화 이펙트 재생 - Player 기준
    void PlayUltimateActivationEffect();

    // 궁극기 무기 이펙트 부착 - Hammer 기준
    UNiagaraComponent* AttachUltimateWeaponEffect();

    // 궁극기 무기 이펙트 제거
    void DetachUltimateWeaponEffect();

    // ─────────── Timer 관리 ───────────
    // 모든 이펙트 Timer 정리 (Pause/Death/Restart 시 호출)
    void ClearAllEffectTimers();

    // 특정 Timer 정리
    void ClearEffectTimer(FTimerHandle& TimerHandle);

    // ─────────── 이펙트 중단 ───────────
    // 수동 관리 중인 모든 활성 이펙트 중단 및 제거
    void StopAllActiveEffects();
    
    // 특정 활성 이펙트 중단 및 제거
    void StopActiveEffect(UNiagaraComponent* EffectComponent);

private:
    // ─────────── 내부 헬퍼 함수 ───────────
    // 현재 상태에 따라 적절한 이펙트 선택
    const FEffectSpawnSettings& GetCurrentStateEffect(const FEffectStatePair& EffectPair) const;

    // 이펙트 스폰 (공통 로직) - Delay 자동 처리
    UNiagaraComponent* SpawnEffect(const FEffectSpawnSettings& Settings, AActor* TargetActor, FTimerHandle* OutTimerHandle = nullptr);

    // 이펙트 즉시 스폰 (내부 구현)
    UNiagaraComponent* SpawnEffectImmediate(const FEffectSpawnSettings& Settings, AActor* TargetActor);

    // 플레이어 캐릭터 가져오기
    ACPlayerCharacter* GetPlayerCharacter() const;

    // 해머 액터 가져오기
    ACHammer* GetHammer() const;

    // 궁극기 활성 상태 확인
    bool IsUltimateActive() const;

public:
    // ─────────── 에디터 설정 가능한 이펙트들 ───────────
    // 일반 공격 이펙트 (3단 콤보) - Hammer의 Joy_WeaponVFX 소켓 기준
    UPROPERTY(EditAnywhere, Category = "Effect|Attack")
    TArray<FEffectStatePair> ComboAttackEffects;

    // 스핀 공격 이펙트 - Hammer의 Joy_WeaponVFX 소켓 기준
    UPROPERTY(EditAnywhere, Category = "Effect|Spin")
    FEffectStatePair SpinAttackEffect;

    // 스핀 피니셔 이펙트 - Hammer의 Joy_WeaponVFX 소켓 기준
    UPROPERTY(EditAnywhere, Category = "Effect|Spin")
    FEffectStatePair SpinFinisherEffect;

    // 커맨드 Launch 이펙트 세트 (먼지, 크랙, 무기 스윙) - Hammer의 Joy_WeaponVFX 소켓 기준
    UPROPERTY(EditAnywhere, Category = "Effect|Command")
    TArray<FEffectStatePair> CommandLaunchEffects;

    // 커맨드 Slam 이펙트 세트 (먼지, 크랙, 무기 스윙) - Hammer의 Joy_WeaponVFX 소켓 기준
    UPROPERTY(EditAnywhere, Category = "Effect|Command")
    TArray<FEffectStatePair> CommandSlamEffects;

    // 대쉬 이펙트 - Player 캐릭터 기준
    UPROPERTY(EditAnywhere, Category = "Effect|Dash")
    FEffectStatePair DashEffect;

    // 궁극기 활성화 시 재생될 이펙트 - Player 캐릭터 기준
    UPROPERTY(EditAnywhere, Category = "Effect|Ultimate")
    FEffectSpawnSettings UltimateActivationEffect;

    // 궁극기 무기 이펙트 - Hammer의 Joy_WeaponVFX 소켓 기준
    UPROPERTY(EditAnywhere, Category = "Effect|Ultimate")
    FEffectSpawnSettings UltimateWeaponEffect;

private:
    // ─────────── Timer Handles ───────────
    // 일반 공격 이펙트 Timers
    TArray<FTimerHandle> ComboAttackTimers;

    // 스핀 공격 Timer
    FTimerHandle SpinAttackTimer;

    // 스핀 피니셔 Timer
    FTimerHandle SpinFinisherTimer;

    // 커맨드 Launch Timers
    TArray<FTimerHandle> CommandLaunchTimers;

    // 커맨드 Slam Timers
    TArray<FTimerHandle> CommandSlamTimers;

    // 대쉬 이펙트 Timer
    FTimerHandle DashEffectTimer;

    // 궁극기 활성화 Timer
    FTimerHandle UltimateActivationTimer;

    // ─────────── 런타임 상태 ───────────
    // 캐시된 플레이어 캐릭터
    TWeakObjectPtr<ACPlayerCharacter> OwnerPlayer;

    // 현재 부착된 궁극기 무기 이펙트
    UPROPERTY()
    TObjectPtr<UNiagaraComponent> ActiveUltimateWeaponEffect = nullptr;

    // 활성 이펙트 컴포넌트들 (수동 관리용)
    UPROPERTY()
    TArray<TObjectPtr<UNiagaraComponent>> ActiveEffects;
};