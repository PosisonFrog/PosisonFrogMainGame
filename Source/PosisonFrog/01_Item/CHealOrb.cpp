#include "01_Item/CHealOrb.h"

#include "Components/SphereComponent.h"
#include "DrawDebugHelpers.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Curves/CurveFloat.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"

#include "00_Character/00_Player/CPlayerCharacter.h"
#include "00_Character/02_Component/CHealthComponent.h"
#include "01_Item/CHealOrbDebugStatsSubsystem.h" // ★ 카운터 연동
// #include "Debug/HealOrbDebugStatsSubsystem.h"

/*// PF CSV/세션 로거 자동 감지
#if __has_include("PFDataDump.h")
#include "PFDataDump.h"
#define PF_CSV_AVAILABLE 1
#else
#define PF_CSV_AVAILABLE 0
#endif
#if __has_include("PFLogFileSink.h")
#include "PFLogFileSink.h"
#define PF_LOG_SESSION 1
#else
#define PF_LOG_SESSION 0
#endif

// 콘솔 변수: 라인/스피어 디버그
static TAutoConsoleVariable<int32> CVarPFHealOrbDebug(
    TEXT("pf.healorb.debug"), 0,
    TEXT("HealOrb debug draw (0:off, 1:on)"),
    ECVF_Cheat);

// CSV 세션 파일 헬퍼
namespace HealOrbCSV
{
#if PF_CSV_AVAILABLE
    static TUniquePtr<PFData::FCsv> GCsv;
    static bool bHeader = false;

    static FString SessionDir()
    {
#if PF_LOG_SESSION
        return PFLogFile::SessionDir();
#else
        return FPaths::ProjectSavedDir() / TEXT("HealOrb");
#endif
    }

    static void Ensure()
    {
        if (GCsv) return;
        IFileManager::Get().MakeDirectory(*SessionDir(), true);
        GCsv = MakeUnique<PFData::FCsv>(SessionDir() / TEXT("heal_orb_events.csv"));
        if (!bHeader)
        {
            GCsv->SetHeader({ TEXT("time"), TEXT("event"), TEXT("orb"), TEXT("target"), TEXT("pos"), TEXT("detail") });
            bHeader = true;
        }
    }

    static void Write(UWorld* World, const FString& Ev, const FString& Orb, const FString& Target, const FString& Pos, const FString& Detail)
    {
        Ensure();
        GCsv->AddRow({ FDateTime::Now().ToString(), Ev, Orb, Target, Pos, Detail });

        // ★ 실시간 카운트 증가
        if (World)
        {
            if (UHealOrbDebugStatsSubsystem* Stats = World->GetSubsystem<UHealOrbDebugStatsSubsystem>())
            {
                using E = EHealOrbEvent;
                if (Ev == TEXT("spawn"))        Stats->Increment(E::Spawn);
                else if (Ev == TEXT("detect_begin")) Stats->Increment(E::DetectBegin);
                else if (Ev == TEXT("detect_end"))   Stats->Increment(E::DetectEnd);
                else if (Ev == TEXT("repath"))       Stats->Increment(E::Repath);
                else if (Ev == TEXT("heal"))         Stats->Increment(E::Heal);
                else if (Ev == TEXT("expire"))       Stats->Increment(E::Expire);
            }
        }
    }
#else
    static bool bHeader = false;
    static FString FilePath()
    {
        const FString Dir = FPaths::ProjectSavedDir() / TEXT("HealOrb");
        IFileManager::Get().MakeDirectory(*Dir, true);
        return Dir / TEXT("heal_orb_events.csv");
    }
    static void Ensure()
    {
        if (bHeader) return;
        const FString Header = TEXT("time,event,orb,target,pos,detail\n");
        FFileHelper::SaveStringToFile(Header, *FilePath(), FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);
        bHeader = true;
    }
    static void Write(UWorld* World, const FString& Ev, const FString& Orb, const FString& Target, const FString& Pos, const FString& Detail)
    {
        Ensure();
        const FString Line = FString::Printf(TEXT("%s,%s,%s,%s,\"%s\",\"%s\"\n"),
            *FDateTime::Now().ToString(), *Ev, *Orb, *Target, *Pos, *Detail);
        FFileHelper::SaveStringToFile(Line, *FilePath(), FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);

        if (World)
        {
            if (UHealOrbDebugStatsSubsystem* Stats = World->GetSubsystem<UHealOrbDebugStatsSubsystem>())
            {
                using E = EHealOrbEvent;
                if (Ev == TEXT("spawn"))        Stats->Increment(E::Spawn);
                else if (Ev == TEXT("detect_begin")) Stats->Increment(E::DetectBegin);
                else if (Ev == TEXT("detect_end"))   Stats->Increment(E::DetectEnd);
                else if (Ev == TEXT("repath"))       Stats->Increment(E::Repath);
                else if (Ev == TEXT("heal"))         Stats->Increment(E::Heal);
                else if (Ev == TEXT("expire"))       Stats->Increment(E::Expire);
            }
        }
    }
#endif
} */// namespace

/* ========== ctor/BeginPlay/Tick ========== */

ACHealOrb::ACHealOrb()
{
    PrimaryActorTick.bCanEverTick = true;
    SetActorTickEnabled(false);

    Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
    SetRootComponent(Sphere);
    Sphere->InitSphereRadius(SphereRadius);
    Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Sphere->SetCollisionObjectType(ECC_WorldDynamic);
    Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    Sphere->SetGenerateOverlapEvents(true);

    DetectSphere = CreateDefaultSubobject<USphereComponent>(TEXT("DetectSphere"));
    DetectSphere->SetupAttachment(Sphere);
    DetectSphere->InitSphereRadius(DetectRadius);
    DetectSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    DetectSphere->SetCollisionObjectType(ECC_WorldDynamic);
    DetectSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    DetectSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    DetectSphere->SetGenerateOverlapEvents(true);
}

void ACHealOrb::BeginPlay()
{
    Super::BeginPlay();

    Sphere->OnComponentBeginOverlap.AddDynamic(this, &ACHealOrb::OnHealBeginOverlap);
    DetectSphere->OnComponentBeginOverlap.AddDynamic(this, &ACHealOrb::OnDetectBeginOverlap);
    DetectSphere->OnComponentEndOverlap.AddDynamic(this, &ACHealOrb::OnDetectEndOverlap);

    ApplySpeedCurvePresetIfNeeded();

    if (bUseHoverConform)
    {
        SetActorLocation(GetActorLocation() + FVector(0, 0, SpawnLiftZ));
        AdjustSpawnOnSlope();
    }

    ValidateTargetOrSleep();
    CsvLog_Spawn();
}

void ACHealOrb::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    LifeAcc += DeltaTime;
    UpdateExpireTimers(DeltaTime);
    ValidateTargetOrSleep();
    if (!IsValid(TargetPlayer))
        return;

    if (!CheapTickGatePass(DeltaTime))
        return;

    const bool bHasLOS = HasLineOfSightToTarget();
    bLastHasLOS = bHasLOS; // HUD 캐시
    TimeSinceDetectLost = bTargetInDetect ? 0.f : (TimeSinceDetectLost + DeltaTime);

    TimeSinceRepath += DeltaTime;
    if (!bHasLOS && bUseNavMesh && TimeSinceRepath >= RepathInterval)
    {
        RebuildPath();
        TimeSinceRepath = 0.f;
        CsvLog_Repath(PathPoints.Num());
    }

    const bool bHasPath = (PathPoints.Num() > 0 && PathIndex < PathPoints.Num());
    const bool bPathExpired = (!bHasPath) || (TimeSinceDetectLost > PathHoldTime);
    if (!bTargetInDetect && !bHasLOS && bPathExpired)
    {
        CsvLog_Expire(TEXT("LostTargetAndPathExpired"));
        TargetPlayer = nullptr;
        PathPoints.Reset(); PathIndex = 0;
        SetActorTickEnabled(false);
        Velocity = FVector::ZeroVector;
        return;
    }

    const FVector OldVel = Velocity;
    if (bHasLOS || PathPoints.Num() == 0)  FollowSteering(DeltaTime);
    else                                 FollowPath(DeltaTime);

    if (bUseHoverConform)
        MaintainHover(DeltaTime);

    if (IsDebugEnabled())
        DrawDebugAll(OldVel, bHasLOS);
}

/* ========== 풀 API ========== */

void ACHealOrb::ResetRuntimeState()
{
    bAlreadyHealed = false;
    Velocity = FVector::ZeroVector;
    PathPoints.Reset(); PathIndex = 0;
    TimeSinceRepath = 0.f;
    bTargetInDetect = false;
    TimeSinceDetectLost = 0.f;
    NoGroundFrames = 0;
    CheapTickAcc = 0.f;
    LifeAcc = 0.f;
    NoTargetAcc = 0.f;
    LastTargetSpeed = 0.f;
    bLastHasLOS = false;
}

void ACHealOrb::ActivateAt(const FVector& SpawnLoc, ACPlayerCharacter* Target)
{
    SetActorLocation(SpawnLoc);
    SetActorHiddenInGame(false);
    SetActorEnableCollision(true);

    ResetRuntimeState();
    ApplySpeedCurvePresetIfNeeded();

    if (bUseHoverConform)
    {
        SetActorLocation(SpawnLoc + FVector(0, 0, SpawnLiftZ));
        AdjustSpawnOnSlope();
    }

    ForceSetTarget(Target);
    CsvLog_Spawn();
}

void ACHealOrb::Deactivate()
{
    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);
    SetActorTickEnabled(false);

    TargetPlayer = nullptr;
    ResetRuntimeState();
}

void ACHealOrb::ForceSetTarget(ACPlayerCharacter* InPlayer)
{
    TargetPlayer = InPlayer;
    bTargetInDetect = IsValid(InPlayer);
    TimeSinceDetectLost = 0.f;
    if (IsValid(InPlayer))
    {
        SetActorTickEnabled(true);
        CheapTickGateBegin();
    }
}

/* ========== Overlaps ========== */

void ACHealOrb::OnDetectBeginOverlap(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
    if (!Other || Other == this) return;

    if (ACPlayerCharacter* Player = Cast<ACPlayerCharacter>(Other))
    {
        TargetPlayer = Player;
        bTargetInDetect = true;
        TimeSinceDetectLost = 0.f;
        SetActorTickEnabled(true);
        CheapTickGateBegin();
        CsvLog_Detect(true);
    }
}

void ACHealOrb::OnDetectEndOverlap(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, int32)
{
    if (Other == TargetPlayer)
    {
        bTargetInDetect = false;
        CsvLog_Detect(false);
    }
}

void ACHealOrb::OnHealBeginOverlap(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
    if (bAlreadyHealed) return;
    if (!Other || Other == this) return;
    if (Other != TargetPlayer) return;

    if (UCHealthComponent* Health = Other->FindComponentByClass<UCHealthComponent>())
    {
        Health->Healing(HealAmount);
        bAlreadyHealed = true;
        CsvLog_Heal();

        if (UCHealOrbDebugStatsSubsystem* Pool = GetWorld()->GetSubsystem<UCHealOrbDebugStatsSubsystem>())
        {
            //Pool->Release(this);
            return;
        }

        if (bDestroyOnHeal) Destroy();
        else Deactivate();  
    }
}

/* ========== 시야/경로/이동 ========== */

bool ACHealOrb::HasLineOfSightToTarget() const
{
    if (!IsValid(TargetPlayer)) return false;

    FHitResult Hit;
    FCollisionQueryParams P(SCENE_QUERY_STAT(HealOrbLOS), false, this);
    P.AddIgnoredActor(this);

    const FVector From = GetActorLocation();
    const FVector To = TargetPlayer->GetActorLocation();

    const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, P);
    return (!bHit || Hit.GetActor() == TargetPlayer);
}

FVector ACHealOrb::ComputeDesiredDir() const
{
    if (!IsValid(TargetPlayer)) return FVector::ZeroVector;
    FVector To = TargetPlayer->GetActorLocation() - GetActorLocation();
    To.Z = 0.f;
    return To.GetSafeNormal();
}

float ACHealOrb::GetDistToTarget2D() const
{
    return IsValid(TargetPlayer)
        ? FVector::Dist2D(GetActorLocation(), TargetPlayer->GetActorLocation())
        : FLT_MAX;
}

FVector ACHealOrb::AvoidObstacles(const FVector& DesiredDir) const
{
    const FVector Pos = GetActorLocation();
    const FVector Right = FVector::CrossProduct(DesiredDir, FVector::UpVector).GetSafeNormal();

    auto Probe = [&](const FVector& From, const FVector& Dir, float Len)->bool
        {
            FHitResult Hit;
            FCollisionQueryParams P(SCENE_QUERY_STAT(HealOrbProbe), false, this);
            P.AddIgnoredActor(this);
            return GetWorld()->LineTraceSingleByChannel(Hit, From, From + Dir * Len, ECC_Visibility, P)
                && Hit.bBlockingHit;
        };

    const bool bFrontBlocked = Probe(Pos, DesiredDir, ProbeLength);
    if (!bFrontBlocked) return DesiredDir;

    const bool bLeftBlocked = Probe(Pos + Right * -SideProbeOffset, DesiredDir, ProbeLength);
    const bool bRightBlocked = Probe(Pos + Right * SideProbeOffset, DesiredDir, ProbeLength);

    const FVector TangentLeft = (DesiredDir + Right * -0.9f).GetSafeNormal();
    const FVector TangentRight = (DesiredDir + Right * 0.9f).GetSafeNormal();

    if (bLeftBlocked && !bRightBlocked) return TangentRight;
    if (!bLeftBlocked && bRightBlocked) return TangentLeft;
    return bRightBlocked ? TangentLeft : TangentRight;
}

FVector ACHealOrb::ComputeSeparationForce() const
{
    if (!bUseSeparation) return FVector::ZeroVector;

    TArray<FOverlapResult> Hits;
    FCollisionObjectQueryParams ObjParams; ObjParams.AddObjectTypesToQuery(ECC_WorldDynamic);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(HealOrbSep), false, this);
    const FCollisionShape Shape = FCollisionShape::MakeSphere(SeparationRadius);

    FVector Sum = FVector::ZeroVector;
    const FVector Center = GetActorLocation();

    if (GetWorld()->OverlapMultiByObjectType(Hits, Center, FQuat::Identity, ObjParams, Shape, Query))
    {
        for (const FOverlapResult& R : Hits)
        {
            const AActor* A = R.GetActor();
            if (!A || A == this) continue;
            if (!A->IsA(StaticClass())) continue;

            FVector ToMe = Center - A->GetActorLocation();
            ToMe.Z = 0.f;
            const float Dist = ToMe.Size();
            if (Dist < KINDA_SMALL_NUMBER) continue;

            const float Weight = 1.f - FMath::Clamp(Dist / SeparationRadius, 0.f, 1.f);
            Sum += (ToMe / Dist) * Weight;
        }
    }
    return Sum.GetSafeNormal() * SeparationStrength;
}

float ACHealOrb::EvalSpeedCurve(float /*DeltaTime*/)
{
    if (!SpeedCurve || SpeedCurveMode == ESpeedCurveMode::None)
        return 1.f;

    if (SpeedCurveMode == ESpeedCurveMode::ByDistanceToTarget)
    {
        const float Dist = FMath::Clamp(GetDistToTarget2D(), 0.f, DetectRadius);
        const float T = (DetectRadius <= KINDA_SMALL_NUMBER) ? 1.f : (1.f - (Dist / DetectRadius));
        return FMath::Max(0.f, SpeedCurve->GetFloatValue(T));
    }
    else
    {
        constexpr float Window = 5.f;
        const float T = FMath::Clamp(LifeAcc / Window, 0.f, 1.f);
        return FMath::Max(0.f, SpeedCurve->GetFloatValue(T));
    }
}

void ACHealOrb::FollowSteering(float DeltaTime)
{
    if (!IsValid(TargetPlayer)) return;

    FVector Dir = AvoidObstacles(ComputeDesiredDir());
    float TargetSpeed = MaxSpeed;
    const float Dist2D = GetDistToTarget2D();

    if (Dist2D < ArriveRadius)
    {
        const float Lerp = FMath::GetMappedRangeValueClamped(FVector2D(0.f, ArriveRadius), FVector2D(0.f, 1.f), Dist2D);
        TargetSpeed *= Lerp;
    }

    TargetSpeed *= EvalSpeedCurve(DeltaTime);
    LastTargetSpeed = TargetSpeed; // HUD 표시용

    const FVector DesiredVel = Dir * TargetSpeed;

    if (TurnAssist > 0.f)
        Velocity = FMath::VInterpTo(Velocity, DesiredVel, DeltaTime, FMath::Clamp(TurnAssist / FMath::Max(KINDA_SMALL_NUMBER, DeltaTime), 0.f, 60.f));

    const FVector ToAdd = (DesiredVel - Velocity);
    const FVector Clamped = ToAdd.GetClampedToMaxSize(Accel * DeltaTime);
    Velocity += Clamped;

    Velocity += ComputeSeparationForce() * DeltaTime;
    Velocity = Velocity.GetClampedToMaxSize(MaxSpeed * 1.5f);

    FVector Delta = Velocity * DeltaTime;
    FVector Adjusted = Delta;
    if (bUseStepCorrection && TryStepUp(Delta, Adjusted))
        Delta = Adjusted;

    FHitResult Hit;
    AddActorWorldOffset(Delta, true, &Hit);
    if (Hit.bBlockingHit)
    {
        FVector Slide = FVector::VectorPlaneProject(Delta, Hit.Normal);
        AddActorWorldOffset(Slide, true);
    }
}

void ACHealOrb::RebuildPath()
{
    PathPoints.Reset();
    PathIndex = 0;

    if (!bUseNavMesh || !IsValid(TargetPlayer)) return;

    if (UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld()))
    {
        const FVector Start = GetActorLocation();
        const FVector Goal = TargetPlayer->GetActorLocation();

        if (UNavigationPath* Path = NavSys->FindPathToLocationSynchronously(GetWorld(), Start, Goal))
        {
            for (const FNavPathPoint& P : Path->GetPath()->GetPathPoints())
                PathPoints.Add(P.Location);

            if (PathPoints.Num() >= 2 && FVector::Dist2D(PathPoints[0], Start) < 50.f)
                PathIndex = 1;
        }
    }
}

void ACHealOrb::FollowPath(float DeltaTime)
{
    if (PathPoints.Num() == 0 || PathIndex >= PathPoints.Num())
        return;

    const FVector Cur = GetActorLocation();

    // 코너 스킵
    for (int32 i = FMath::Min(PathIndex + 2, PathPoints.Num() - 1); i > PathIndex; --i)
    {
        FHitResult Hit;
        FCollisionQueryParams P(SCENE_QUERY_STAT(HealOrbCornerCut), false, this);
        P.AddIgnoredActor(this);
        if (!GetWorld()->LineTraceSingleByChannel(Hit, Cur, PathPoints[i], ECC_Visibility, P))
        {
            PathIndex = i;
            break;
        }
    }

    FVector To = PathPoints[PathIndex] - Cur; To.Z = 0.f;
    FVector Dir = AvoidObstacles(To.GetSafeNormal());

    float TargetSpeed = MaxSpeed * EvalSpeedCurve(DeltaTime);
    LastTargetSpeed = TargetSpeed;

    const FVector DesiredVel = Dir * TargetSpeed;

    const FVector ToAdd = (DesiredVel - Velocity);
    const FVector Clamped = ToAdd.GetClampedToMaxSize(Accel * DeltaTime);
    Velocity += Clamped;

    Velocity += ComputeSeparationForce() * DeltaTime;
    Velocity = Velocity.GetClampedToMaxSize(MaxSpeed * 1.5f);

    FVector Delta = Velocity * DeltaTime;
    FVector Adjusted = Delta;
    if (bUseStepCorrection && TryStepUp(Delta, Adjusted))
        Delta = Adjusted;

    FHitResult Hit;
    AddActorWorldOffset(Delta, true, &Hit);
    if (Hit.bBlockingHit)
    {
        FVector Slide = FVector::VectorPlaneProject(Delta, Hit.Normal);
        AddActorWorldOffset(Slide, true);
    }

    if (FVector::Dist2D(Cur, PathPoints[PathIndex]) < WaypointReachRadius)
        ++PathIndex;
}

/* ========== Hover/Step ========= */

void ACHealOrb::AdjustSpawnOnSlope()
{
    UWorld* World = GetWorld(); if (!World) return;
    const float SweepR = FMath::Max(1.f, SphereRadius - 2.f);
    const FVector Start = GetActorLocation() + FVector(0, 0, HoverTraceUp);
    const FVector End = GetActorLocation() - FVector(0, 0, HoverTraceDown);

    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(HealOrb_SpawnSnap), false, this);
    const bool bHit = World->SweepSingleByChannel(
        Hit, Start, End, FQuat::Identity, ECC_Visibility,
        FCollisionShape::MakeSphere(SweepR), Params);

    if (bHit && Hit.bBlockingHit)
    {
        const FVector Target = Hit.ImpactPoint + Hit.ImpactNormal * FMath::Max(HoverHeight, 8.f);
        SetActorLocation(Target, true);
    }
}

float ACHealOrb::SnapZToGround(float CurrentZ, float DeltaTime) const
{
    return CurrentZ - MaxStepDownPerTick * DeltaTime;
}

void ACHealOrb::MaintainHover(float DeltaTime)
{
    UWorld* World = GetWorld(); if (!World) return;

    const float SweepR = FMath::Max(1.f, SphereRadius - 2.f);
    const FVector Cur = GetActorLocation();
    const FVector Start = Cur + FVector(0, 0, HoverTraceUp);
    const FVector End = Cur - FVector(0, 0, HoverTraceDown);

    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(HealOrb_HoverSnap), false, this);
    const bool bHit = World->SweepSingleByChannel(
        Hit, Start, End, FQuat::Identity, ECC_Visibility,
        FCollisionShape::MakeSphere(SweepR), Params);

    if (bHit && Hit.bBlockingHit)
    {
        NoGroundFrames = 0;

        const FVector Desired = Hit.ImpactPoint + Hit.ImpactNormal * FMath::Max(HoverHeight, 8.f);
        FVector Target = Cur;
        float DesiredZ = Desired.Z;

        const float MinAllowedZ = SnapZToGround(Cur.Z, DeltaTime);
        DesiredZ = FMath::Max(DesiredZ, MinAllowedZ);

        Target.Z = DesiredZ;
        const FVector NewLoc = FMath::VInterpTo(Cur, Target, DeltaTime, HoverInterpSpeed);
        SetActorLocation(NewLoc, true);
    }
    else
    {
        NoGroundFrames++;
        if (NoGroundFrames >= MaxNoGroundFramesBeforeFall)
            AddActorWorldOffset(FVector(0, 0, -FallSpeedWhenNoGround * DeltaTime), true);
    }
}

/* ========== Tick LOD / Expire ========= */

void ACHealOrb::CheapTickGateBegin() { CheapTickAcc = 0.f; }

bool ACHealOrb::CheapTickGatePass(float DeltaTime)
{
    if (!bUseCheapTickWhenFar || !IsValid(TargetPlayer)) return true;

    const float Dist = GetDistToTarget2D();
    if (Dist <= CheapTickDistance) return true;

    CheapTickAcc += DeltaTime;
    if (CheapTickAcc >= CheapTickInterval)
    {
        CheapTickAcc = 0.f;
        return true;
    }
    return false;
}

void ACHealOrb::UpdateExpireTimers(float DeltaTime)
{
    if (!bAutoExpire) return;

    if (IsValid(TargetPlayer)) NoTargetAcc = 0.f;
    else                       NoTargetAcc += DeltaTime;

    if (NoTargetAcc >= ExpireAfterNoTargetSeconds)
    {
        CsvLog_Expire(TEXT("AutoExpire_NoTarget"));
        if (UCHealOrbDebugStatsSubsystem* Pool = GetWorld()->GetSubsystem<UCHealOrbDebugStatsSubsystem>())
            //Pool->Release(this);
        else
            Destroy();
    }
}

void ACHealOrb::ValidateTargetOrSleep()
{
    if (!IsValid(TargetPlayer))
    {
        SetActorTickEnabled(false);
        Velocity = FVector::ZeroVector;
    }
}

/* ========== Speed Curve ========= */

void ACHealOrb::ApplySpeedCurvePresetIfNeeded()
{
    if (SpeedCurveMode == ESpeedCurveMode::None)
        return;

    if (SpeedCurve && SpeedCurve->FloatCurve.GetNumKeys() > 0)
        return;

    SpeedCurve = NewObject<UCurveFloat>(this, TEXT("HealOrbAutoCurve"));
    FRichCurve& RC = SpeedCurve->FloatCurve;
    RC.Reset();

    auto K = [&](float X, float Y) {
        const auto& Key = RC.AddKey(X, Y);
        RC.SetKeyInterpMode(Key, RCIM_Cubic);
    };

    switch (SpeedCurvePreset)
    {
    case ESpeedCurvePreset::AggressiveEase:
        K(0.00f, 0.20f);  K(0.20f, 1.00f);  K(0.80f, 1.00f);  K(1.00f, 0.20f);
        break;
    case ESpeedCurvePreset::RushIn:
        K(0.00f, 0.10f);  K(0.15f, 1.00f);  K(0.60f, 0.95f);  K(1.00f, 0.90f);
        break;
    case ESpeedCurvePreset::RushOut:
        K(0.00f, 0.60f);  K(0.60f, 1.00f);  K(0.90f, 0.50f);  K(1.00f, 0.30f);
        break;
    case ESpeedCurvePreset::Pulse:
        K(0.00f, 0.30f);  K(0.25f, 0.95f);  K(0.50f, 0.70f);  K(0.75f, 0.95f);  K(1.00f, 0.40f);
        break;
    case ESpeedCurvePreset::SoftEase:
    default:
        K(0.00f, 0.35f);  K(0.30f, 0.90f);  K(0.70f, 0.90f);  K(1.00f, 0.40f);
        break;
    }
}

/* ========== Debug Draw ========= */

bool ACHealOrb::IsDebugEnabled() const
{
    return bDebugDraw;
    //|| (CVarPFHealOrbDebug.GetValueOnAnyThread() != 0);
}

void ACHealOrb::DrawDebugAll(const FVector& PrevVel, bool bHasLOS)
{
    UWorld* World = GetWorld(); if (!World) return;

    const FVector P = GetActorLocation();
    const FColor  LOSCol = bHasLOS ? FColor::Cyan : FColor::Red;

    if (IsValid(TargetPlayer))
        DrawDebugLine(World, P, TargetPlayer->GetActorLocation(), LOSCol, false, DebugDuration, 0, DebugThickness);

    DrawDebugLine(World, P, P + Velocity * 0.05f, FColor::Green, false, DebugDuration, 0, DebugThickness);
    DrawDebugLine(World, P, P + PrevVel * 0.05f, FColor::Silver, false, DebugDuration, 0, 1.0f);

    DrawDebugCircle(World, P, ArriveRadius, 32, FColor::Yellow, false, DebugDuration, 0, DebugThickness, FVector(1, 0, 0), FVector(0, 1, 0), false);

    if (bUseSeparation)
        DrawDebugCircle(World, P, SeparationRadius, 32, FColor::Purple, false, DebugDuration, 0, 1.0f, FVector(1, 0, 0), FVector(0, 1, 0), false);

    if (PathPoints.Num() > 0 && PathIndex < PathPoints.Num())
    {
        for (int32 i = PathIndex + 1; i < PathPoints.Num(); ++i)
            DrawDebugLine(World, PathPoints[i - 1], PathPoints[i], FColor::Orange, false, DebugDuration, 0, 2.0f);

        DrawDebugSphere(World, PathPoints[PathIndex], 16.f, 12, FColor::Orange, false, DebugDuration, 0, 1.0f);
    }
}

/* ========== CSV & 카운트 ========= */

/*FString ACHealOrb::CsvTargetName() const
{
    return IsValid(TargetPlayer) ? TargetPlayer->GetName() : TEXT("None");
}

void ACHealOrb::CsvLog_Spawn()
{
    HealOrbCSV::Write(GetWorld(), TEXT("spawn"), GetName(), CsvTargetName(), GetActorLocation().ToString(), TEXT(""));
}
void ACHealOrb::CsvLog_Detect(bool bBegin)
{
    HealOrbCSV::Write(GetWorld(), bBegin ? TEXT("detect_begin") : TEXT("detect_end"),
        GetName(), CsvTargetName(), GetActorLocation().ToString(), TEXT(""));
}
void ACHealOrb::CsvLog_Repath(int32 NumPts)
{
    HealOrbCSV::Write(GetWorld(), TEXT("repath"), GetName(), CsvTargetName(), GetActorLocation().ToString(),
        FString::Printf(TEXT("points=%d"), NumPts));
}
void ACHealOrb::CsvLog_Heal()
{
    HealOrbCSV::Write(GetWorld(), TEXT("heal"), GetName(), CsvTargetName(), GetActorLocation().ToString(),
        FString::Printf(TEXT("amount=%.1f"), HealAmount));
}
void ACHealOrb::CsvLog_Expire(const TCHAR* Reason)
{
    HealOrbCSV::Write(GetWorld(), TEXT("expire"), GetName(), CsvTargetName(), GetActorLocation().ToString(), Reason);
}*/

/* ========== HUD 스냅샷 ========= */

void ACHealOrb::GetDebugInfo(FHealOrbDebugInfo& Out) const
{
    Out.Name = GetName();
    Out.bHasTarget = IsValid(TargetPlayer);
    Out.bInDetect = bTargetInDetect;
    Out.bHasLOS = bLastHasLOS;

    Out.Dist2D = GetDistToTarget2D();
    Out.PathIndex = PathIndex;
    Out.PathNum = PathPoints.Num();

    Out.Speed = Velocity.Size2D();
    Out.MaxSpeedVal = MaxSpeed;
    Out.AccelVal = Accel;

    Out.LifeSec = LifeAcc;
    Out.NoGroundFramesInt = NoGroundFrames;
    Out.CheapTickAccSec = CheapTickAcc;

    Out.Location = GetActorLocation();
    Out.VelocityVec = Velocity;

    Out.CurveMode = (uint8)SpeedCurveMode;
    Out.CurvePreset = (uint8)SpeedCurvePreset;
    Out.ArriveRadiusVal = ArriveRadius;
    Out.TurnAssistVal = TurnAssist;
    Out.DetectRadiusVal = DetectRadius;
    Out.RepathIntervalVal = RepathInterval;
}
