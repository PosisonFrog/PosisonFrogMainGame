// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CHealOrb.generated.h"

class USphereComponent;
class ACPlayerCharacter;
class UCurveFloat;

UENUM()
enum class ESpeedCurveMode : uint8
{
    None,
    ByDistanceToTarget,
    ByLifetime
};

UENUM()
enum class ESpeedCurvePreset : uint8
{
    None,
    SoftEase,
    AggressiveEase,
    RushIn,
    RushOut,
    Pulse
};

// ── HUD용 디버그 스냅샷 ─────────────────────────────────────────
USTRUCT()
struct FHealOrbDebugInfo
{
    GENERATED_BODY()

    UPROPERTY() FString Name;
    UPROPERTY() bool    bHasTarget = false;
    UPROPERTY() bool    bInDetect = false;
    UPROPERTY() bool    bHasLOS = false;

    UPROPERTY() float   Dist2D = 0.f;
    UPROPERTY() int32   PathIndex = 0;
    UPROPERTY() int32   PathNum = 0;

    UPROPERTY() float   Speed = 0.f;   // |Velocity|
    UPROPERTY() float   MaxSpeedVal = 0.f;   // MaxSpeed
    UPROPERTY() float   AccelVal = 0.f;   // Accel

    UPROPERTY() float   LifeSec = 0.f;
    UPROPERTY() int32   NoGroundFramesInt = 0;
    UPROPERTY() float   CheapTickAccSec = 0.f;

    UPROPERTY() FVector Location = FVector::ZeroVector;
    UPROPERTY() FVector VelocityVec = FVector::ZeroVector;

    // 튜닝/커브 정보(상세 패널용)
    UPROPERTY() uint8   CurveMode = static_cast<uint8>(ESpeedCurveMode::None);
    UPROPERTY() uint8   CurvePreset = static_cast<uint8>(ESpeedCurvePreset::SoftEase);
    UPROPERTY() float   ArriveRadiusVal = 0.f;
    UPROPERTY() float   TurnAssistVal = 0.f;
    UPROPERTY() float   DetectRadiusVal = 0.f;
    UPROPERTY() float   RepathIntervalVal = 0.f;
};

UCLASS()
class POSISONFROG_API ACHealOrb : public AActor
{
    GENERATED_BODY()

public:
    ACHealOrb();

    // 풀 사용 시
    void ActivateAt(const FVector& SpawnLoc, ACPlayerCharacter* Target);
    void Deactivate();

    // 직접 스폰 시
    void ForceSetTarget(ACPlayerCharacter* InPlayer);

    // HUD용 스냅샷
    void GetDebugInfo(FHealOrbDebugInfo& Out) const;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:
    // === Components ===
    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<USphereComponent> Sphere;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<USphereComponent> DetectSphere;

    // === Target/Heal ===
    UPROPERTY(VisibleInstanceOnly, Category = "Heal")
    TObjectPtr<ACPlayerCharacter> TargetPlayer = nullptr;

    UPROPERTY(EditAnywhere, Category = "Heal")
    float HealAmount = 30.f;

    UPROPERTY(EditAnywhere, Category = "Heal")
    bool bDestroyOnHeal = false;

    bool bAlreadyHealed = false;

    // === Movement ===
    UPROPERTY(EditAnywhere, Category = "Chase|Move")
    float MaxSpeed = 800.f;

    UPROPERTY(EditAnywhere, Category = "Chase|Move")
    float Accel = 3000.f;

    UPROPERTY(EditAnywhere, Category = "Chase|Move")
    float ArriveRadius = 120.f;

    UPROPERTY(EditAnywhere, Category = "Chase|Move", meta = (ClampMin = "0", ClampMax = "1"))
    float TurnAssist = 0.25f;

    // Avoid
    UPROPERTY(EditAnywhere, Category = "Chase|Avoid")
    float ProbeLength = 140.f;

    UPROPERTY(EditAnywhere, Category = "Chase|Avoid")
    float SideProbeOffset = 70.f;

    // Nav
    UPROPERTY(EditAnywhere, Category = "Chase|Nav")
    bool bUseNavMesh = true;

    UPROPERTY(EditAnywhere, Category = "Chase|Nav")
    float RepathInterval = 0.30f;

    UPROPERTY(EditAnywhere, Category = "Chase|Nav")
    float WaypointReachRadius = 80.f;

    // Detect Reacquire
    UPROPERTY(EditAnywhere, Category = "Detect")
    float PathHoldTime = 3.0f;

    UPROPERTY(EditAnywhere, Category = "Detect")
    float DetectRadius = 800.0f;

    // Collision (heal trigger)
    UPROPERTY(EditAnywhere, Category = "Collision")
    float SphereRadius = 60.0f;

    // Separation
    UPROPERTY(EditAnywhere, Category = "Separation")
    bool bUseSeparation = true;

    UPROPERTY(EditAnywhere, Category = "Separation")
    float SeparationRadius = 120.f;

    UPROPERTY(EditAnywhere, Category = "Separation")
    float SeparationStrength = 1200.f;

    // Hover/Step
    UPROPERTY(EditAnywhere, Category = "Grounding|Hover")
    bool bUseHoverConform = true;

    UPROPERTY(EditAnywhere, Category = "Grounding|Hover")
    float HoverHeight = 35.f;

    UPROPERTY(EditAnywhere, Category = "Grounding|Hover")
    float SpawnLiftZ = 30.f;

    UPROPERTY(EditAnywhere, Category = "Grounding|Hover")
    float HoverInterpSpeed = 12.f;

    UPROPERTY(EditAnywhere, Category = "Grounding|Hover")
    float HoverTraceUp = 60.f;

    UPROPERTY(EditAnywhere, Category = "Grounding|Hover")
    float HoverTraceDown = 220.f;

    UPROPERTY(EditAnywhere, Category = "Grounding|Step")
    bool bUseStepCorrection = true;

    UPROPERTY(EditAnywhere, Category = "Grounding|Step")
    float StepUpMaxHeight = 45.f;

    UPROPERTY(EditAnywhere, Category = "Grounding|Step")
    float StepForwardProbeScale = 1.2f;

    UPROPERTY(EditAnywhere, Category = "Grounding|Step")
    float MaxStepDownPerTick = 60.f;

    UPROPERTY(EditAnywhere, Category = "Grounding|NoGround")
    int32 MaxNoGroundFramesBeforeFall = 6;

    UPROPERTY(EditAnywhere, Category = "Grounding|NoGround")
    float FallSpeedWhenNoGround = 250.f;

    // Tick LOD / Expire
    UPROPERTY(EditAnywhere, Category = "Perf")
    bool bUseCheapTickWhenFar = true;

    UPROPERTY(EditAnywhere, Category = "Perf")
    float CheapTickDistance = 1200.f;

    UPROPERTY(EditAnywhere, Category = "Perf")
    float CheapTickInterval = 0.15f;

    UPROPERTY(EditAnywhere, Category = "Lifetime")
    bool bAutoExpire = true;

    UPROPERTY(EditAnywhere, Category = "Lifetime")
    float ExpireAfterNoTargetSeconds = 20.f;

    // Speed curve
    UPROPERTY(EditAnywhere, Category = "Chase|SpeedCurve")
    ESpeedCurveMode SpeedCurveMode = ESpeedCurveMode::None;

    UPROPERTY(EditAnywhere, Category = "Chase|SpeedCurve")
    ESpeedCurvePreset SpeedCurvePreset = ESpeedCurvePreset::SoftEase;

    UPROPERTY(EditAnywhere, Category = "Chase|SpeedCurve")
    bool bAutoGenerateDefaultCurveIfNone = true;

    UPROPERTY(EditAnywhere, Category = "Chase|SpeedCurve")
    TObjectPtr<UCurveFloat> SpeedCurve = nullptr;

    // Debug flags
    UPROPERTY(EditAnywhere, Category = "Debug")
    bool bDebugDraw = false;

    UPROPERTY(EditAnywhere, Category = "Debug")
    float DebugDuration = 0.05f;

    UPROPERTY(EditAnywhere, Category = "Debug")
    float DebugThickness = 1.5f;

private:
    // Overlaps
    UFUNCTION()
    void OnHealBeginOverlap(UPrimitiveComponent* Overlapped, AActor* Other, UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep);
    UFUNCTION()
    void OnDetectBeginOverlap(UPrimitiveComponent* Overlapped, AActor* Other, UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep);
    UFUNCTION()
    void OnDetectEndOverlap(UPrimitiveComponent* Overlapped, AActor* Other, UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex);

    // Move helpers
    bool   HasLineOfSightToTarget() const;
    void   RebuildPath();
    void   FollowSteering(float DeltaTime);
    void   FollowPath(float DeltaTime);

    FVector ComputeDesiredDir() const;
    FVector AvoidObstacles(const FVector& DesiredDir) const;
    FVector ComputeSeparationForce() const;
    float   GetDistToTarget2D() const;

    // Grounding / Step
    void    AdjustSpawnOnSlope();
    void    MaintainHover(float DeltaTime);
    bool    TryStepUp(const FVector& IntendedDelta, FVector& OutAdjustedDelta) const;
    float   SnapZToGround(float CurrentZ, float DeltaTime) const;

    // TickLOD / Expire / Target
    void    CheapTickGateBegin();
    bool    CheapTickGatePass(float DeltaTime);
    void    UpdateExpireTimers(float DeltaTime);
    void    ValidateTargetOrSleep();

    // Speed curve
    void    ApplySpeedCurvePresetIfNeeded();
    float   EvalSpeedCurve(float DeltaTime);

    // Debug draw
    bool    IsDebugEnabled() const;
    void    DrawDebugAll(const FVector& PrevVel, bool bHasLOS);

    // CSV + 실시간 카운터
    void    CsvLog_Spawn();
    void    CsvLog_Detect(bool bBegin);
    void    CsvLog_Repath(int32 NumPts);
    void    CsvLog_Heal();
    void    CsvLog_Expire(const TCHAR* Reason);
    FString CsvTargetName() const;

    // 상태 초기화
    void    ResetRuntimeState();

private:
    // Runtime
    FVector Velocity = FVector::ZeroVector;

    float   TimeSinceRepath = 0.f;
    bool    bTargetInDetect = false;
    float   TimeSinceDetectLost = 0.f;

    TArray<FVector> PathPoints;
    int32  PathIndex = 0;

    // Hover/ground
    mutable int32 NoGroundFrames = 0;

    // Cheap tick
    float   CheapTickAcc = 0.f;

    // Lifetime
    float   LifeAcc = 0.f;
    float   NoTargetAcc = 0.f;

    // HUD 캐시
    bool  bLastHasLOS = false;
    float LastTargetSpeed = 0.f;
};