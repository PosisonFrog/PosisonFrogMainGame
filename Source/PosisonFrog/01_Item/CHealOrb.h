// CHealOrb.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Curves/CurveFloat.h"
#include "CHealOrb.generated.h"

class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;
class UNiagaraSystem;
class USoundBase;
class UCHealOrbPoolSubsystem;

// ===== Enums =====
/** 힐 오브 상태 */
UENUM(BlueprintType)
enum class HealOrbState
{
    Spawn,
    Chase
};

/** 속도 커브 프리셋 */
UENUM(BlueprintType)
enum class EHealOrbSpeedCurvePreset : uint8
{
    Linear,
    EaseIn,         // 느리게 시작 → 가속
    EaseOut,        // 빠르게 시작 → 브레이크
    EaseInOut,      // S-curve
    FastStartBrake, // 초반 튀고 후반 강한 감속
    RubberBand      // 초반 강가속, 중후반 미세 진동 감
};

// ====== Delegates =====
/** HUD/외부 연동용 이벤트 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHealOrbSpawned, AActor*, OrbActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealOrbPicked, AActor*, OrbActor, AActor*, HealedActor);

UCLASS()
class POSISONFROG_API ACHealOrb : public AActor
{
    GENERATED_BODY()

public:
    ACHealOrb();

    /** 오브 활성화(스폰/풀에서 꺼낼 때). PreferredTarget이 있으면 즉시 추적 시작 */
    UFUNCTION(BlueprintCallable, Category="HealOrb")
    void ActivateOrb(AActor* PreferredTarget = nullptr);

    /** 오브 해제(풀 반납 or 파괴). 여러 번 호출돼도 안전 */
    UFUNCTION(BlueprintCallable, Category="HealOrb")
    void ReleaseOrb(bool bReturnToPool = false);

    /** 런타임 파라미터 튜닝 */
    UFUNCTION(BlueprintCallable, Category="HealOrb")
    void SetupParams(float InHealAmount, float InSpeed) { HealAmount = InHealAmount; BaseSpeed = InSpeed; }

    /** 외부에서 타깃을 강제 지정(재획득용) */
    UFUNCTION(BlueprintCallable, Category="HealOrb")
    void SetTarget(AActor* NewTarget);

    // 이벤트: HUD/카운터 연동
    UPROPERTY(BlueprintAssignable, Category="HealOrb|Events")
    FOnHealOrbSpawned OnOrbSpawned;

    UPROPERTY(BlueprintAssignable, Category="HealOrb|Events")
    FOnHealOrbPicked OnOrbPicked;

    /** 풀 매니저가 설정하는 플래그(픽업 시 ReleaseOrb(true) 하도록) */
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="HealOrb|Pool")
    bool bUsePooling = false;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    // ===== 내부 루틴 =====
    void ResetOrbState();
    void BindOverlaps();
    void UnbindOverlaps();
    void EnableCollisions(bool bEnable);
    void UpdateSpeedByCurve(float CurrentDist);
    void UpdateHover(float DeltaTime);
    void AppendCsv(const FString& Line);
    void ApplyCurvePreset(EHealOrbSpeedCurvePreset Preset);
    bool HasLineOfSightToTarget(const FVector& From, const FVector& To) const;
    void EnterSpawnState();
    void EnterChaseState();

    // ===== 오버랩 핸들러 =====
    UFUNCTION()
    void OnPickupOverlap(UPrimitiveComponent* Overlapped, AActor* OtherActor,
                         UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                         bool bFromSweep, const FHitResult& Hit);

    UFUNCTION()
    void OnDetectOverlap(UPrimitiveComponent* Overlapped, AActor* OtherActor,
                         UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                         bool bFromSweep, const FHitResult& Hit);

    UFUNCTION()
    void OnDetectEndOverlap(UPrimitiveComponent* Overlapped, AActor* OtherActor,
                            UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

    UFUNCTION()
    void OnSpawnProjectileStop(const FHitResult& ImpactResult);

private:
    // ===== 컴포넌트 =====
    UPROPERTY(VisibleAnywhere, Category="Components")
    TObjectPtr<USphereComponent> PickupSphere;

    UPROPERTY(VisibleAnywhere, Category="Components")
    TObjectPtr<USphereComponent> DetectSphere;

    UPROPERTY(VisibleAnywhere, Category="Components")
    TObjectPtr<UStaticMeshComponent> VisualMesh;

    UPROPERTY(VisibleAnywhere, Category = "HealOrb|Spawn")
    TObjectPtr<UProjectileMovementComponent> SpawnProjectile = nullptr;

    // ===== Spawn (중력 / 바운스) =====
    UPROPERTY(EditAnywhere, Category = "HealOrb|Spawn")
    float SpawnUpSpeed = 600.0f;
    
    UPROPERTY(EditAnywhere, Category = "HealOrb|Spawn")
    float SpawnHorizontalSpeed = 200.0f;

    UPROPERTY(EditAnywhere, Category = "HealOrb|Spawn")
    float SpawnBounciness = 0.35f;

    UPROPERTY(EditAnywhere, Category = "HealOrb|Spawn")
    float SpawnFriction = 0.2f;

    // 바운드 끝나면 바로 먹기 허용 : OnSpawnProjectileStop()에서만 false
    // 추적 시작 전에는 절대 먹히지 않음 : EnterChaseState()에서만 false
    bool bSpawnPickupLocked = false;
    bool bChaseAllowed = false;

    FTimerHandle SpawnDelayHandle;
    
    // ===== 추적/치유 파라미터 =====
    UPROPERTY(EditAnywhere, Category="HealOrb|Chase", meta=(ClampMin="0.0"))
    float PickupRadius = 60.f;

    UPROPERTY(EditAnywhere, Category="HealOrb|Chase", meta=(ClampMin="0.0"))
    float DetectRadius = 300.f;

    UPROPERTY(EditAnywhere, Category="HealOrb|Chase", meta=(ClampMin="0.0"))
    float BaseSpeed = 550.f;

    UPROPERTY(EditAnywhere, Category="HealOrb|Chase", meta=(ClampMin="0.0"))
    float HealAmount = 30.f;

    /** Detect End 이후에도 추적 유지할 유예시간(초). 0이면 즉시 해제 */
    UPROPERTY(EditAnywhere, Category="HealOrb|Chase", meta=(ClampMin="0.0"))
    float DetectLossGraceTime = 1.0f;

    /** Detect를 잃었어도 이 거리 이내면 계속 추적 유지 */
    UPROPERTY(EditAnywhere, Category="HealOrb|Chase", meta=(ClampMin="0.0"))
    float KeepChaseMaxDistance = 2000.f;

    // ===== Hover =====
    /** 간단한 단차 보정: 바닥 트레이스로 HoverHeight 유지 */
    UPROPERTY(EditAnywhere, Category="HealOrb|Hover", meta=(ClampMin="0.0"))
    float HoverHeight = 120.f;

    UPROPERTY(EditAnywhere, Category="HealOrb|Hover", meta=(ClampMin="0.0"))
    float HoverTraceLength = 200.f;

    /** 지면 따라 부드러운 Z 보정 속도 */
    UPROPERTY(EditAnywhere, Category="HealOrb|Hover", meta=(ClampMin="0.0"))
    float HoverLerpSpeed = 10.f;

    // ===== 속도 커브 =====
    /** 거리 기반 0..1 진척도로 속도 조절. 외부 자산 우선 */
    UPROPERTY(EditAnywhere, Category="HealOrb|SpeedCurve")
    TObjectPtr<UCurveFloat> SpeedCurveAsset = nullptr;

    /** 자산이 없을 때 사용하는 런타임 커브 */
    UPROPERTY(EditAnywhere, Category="HealOrb|SpeedCurve")
    FRuntimeFloatCurve RuntimeSpeedCurve;

    /** 프리셋 선택 시 RuntimeSpeedCurve에 키를 자동 구성 */
    UPROPERTY(EditAnywhere, Category="HealOrb|SpeedCurve")
    EHealOrbSpeedCurvePreset SpeedCurvePreset = EHealOrbSpeedCurvePreset::EaseOut;

    /** 커브 적용 세기(최종 속도 = BaseSpeed * (CurveValue * Strength)) */
    UPROPERTY(EditAnywhere, Category="HealOrb|SpeedCurve", meta=(ClampMin="0.0"))
    float CurveStrength = 1.0f;

    // ===== 디버그 =====
    UPROPERTY(EditAnywhere, Category="HealOrb|Debug")
    bool bDebugDraw = false;

    UPROPERTY(EditAnywhere, Category="HealOrb|Debug", meta=(ClampMin="0.0"))
    float DebugDrawDuration = 0.05f;

    // ===== CSV 로깅 =====
    UPROPERTY(EditAnywhere, Category="HealOrb|CSV")
    bool bEnableCsvLogging = false;

    /** Saved/HealOrbs/HealOrbLog.csv 등 상대 경로로 기록 */
    UPROPERTY(EditAnywhere, Category="HealOrb|CSV")
    FString CsvRelativePath = TEXT("HealOrbs/HealOrbLog.csv");

    // ===== VFX/SFX =====
    UPROPERTY(EditAnywhere, Category="HealOrb|VFX")
    TObjectPtr<UNiagaraSystem> VFX_Spawn = nullptr;

    UPROPERTY(EditAnywhere, Category="HealOrb|VFX")
    TObjectPtr<UNiagaraSystem> VFX_Pick = nullptr;

    UPROPERTY(EditAnywhere, Category="HealOrb|SFX")
    TObjectPtr<USoundBase> SFX_Spawn = nullptr;

    UPROPERTY(EditAnywhere, Category="HealOrb|SFX")
    TObjectPtr<USoundBase> SFX_Pick = nullptr;

private:
    // ===== 상태 =====
    HealOrbState State = HealOrbState::Spawn;
    
    TWeakObjectPtr<AActor> TargetActor;
    FVector LastKnownTargetLocation = FVector::ZeroVector;
    float   StartDistanceToTarget   = 0.f;
    float   CurrentSpeed            = 0.f;

    bool    bActive   = false; // Tick/추적 활성
    bool    bReleased = false; // Release 재진입 가드
    
    // Detect End 유예
    bool    bDetectLost = false;
    float   DetectLostTimeAcc = 0.f;

    // 안전 파괴/풀반납 타이머
    FTimerHandle SafeDestroyHandle;
};
