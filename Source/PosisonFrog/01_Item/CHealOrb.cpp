// CHealOrb.cpp
#include "01_Item/CHealOrb.h"

#include "01_Item/CHealOrbPoolSubsystem.h"
#include "00_Character/02_Component/00_PlayerComponent/CPlayerHealthComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Engine/CollisionProfile.h"
#include "TimerManager.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

ACHealOrb::ACHealOrb()
{
    PrimaryActorTick.bCanEverTick = true;
    SetActorTickEnabled(false);

    // --- Pickup ---
    PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
    SetRootComponent(PickupSphere);
    PickupSphere->InitSphereRadius(PickupRadius);
    PickupSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    PickupSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    PickupSphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
    PickupSphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
    PickupSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    PickupSphere->SetCollisionResponseToChannel(ECC_GameTraceChannel1,ECR_Overlap);
    PickupSphere->SetGenerateOverlapEvents(true);

    
    // --- Detect ---
    DetectSphere = CreateDefaultSubobject<USphereComponent>(TEXT("DetectSphere"));
    DetectSphere->SetupAttachment(PickupSphere);
    DetectSphere->InitSphereRadius(DetectRadius);
    DetectSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    DetectSphere->SetCollisionObjectType(ECC_WorldDynamic);
    DetectSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    DetectSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    DetectSphere->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Overlap);
    DetectSphere->SetGenerateOverlapEvents(true);

    // --- Visual ---
    VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
    VisualMesh->SetupAttachment(PickupSphere);
    VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // --- Spawn Projectile (중력 / 바운스) ---
    SpawnProjectile = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("SpawnProjectile"));
    SpawnProjectile->bAutoActivate                = false;
    SpawnProjectile->UpdatedComponent             = PickupSphere;
    SpawnProjectile->ProjectileGravityScale       = 1.0f;
    SpawnProjectile->bShouldBounce                = true;
    SpawnProjectile->Bounciness                   = SpawnBounciness;
    SpawnProjectile->Friction                     = SpawnFriction;
    SpawnProjectile->bRotationFollowsVelocity     = false;
    SpawnProjectile->bInitialVelocityInLocalSpace = false;
    SpawnProjectile->bForceSubStepping            = true;
    SpawnProjectile->OnProjectileStop.AddDynamic(this, &ACHealOrb::OnSpawnProjectileStop);

    // 기본 커브 프리셋 구성
    ApplyCurvePreset(SpeedCurvePreset);
}

void ACHealOrb::BeginPlay()
{
    Super::BeginPlay();

    if (bEnableCsvLogging)
    {
        AppendCsv(TEXT("Event,Time,Orb,Target,Dist,Speed"));
    }
}

void ACHealOrb::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (!bActive || bReleased) return;

    if (State == HealOrbState::Spawn)
    {
        UpdateHover(DeltaTime);
        return;
    }

    if (!bChaseAllowed) return;

    UWorld* World = GetWorld();
    AActor* T = TargetActor.Get();

    // 타깃 위치/유지 정책
    if (IsValid(T))
    {
        LastKnownTargetLocation = T->GetActorLocation();
        if (!HasLineOfSightToTarget(GetActorLocation(), LastKnownTargetLocation))
        {
            // LOS가 없어도 추적 지속(옵션). 현재는 유지
        }
    }
    else
    {
        // Detect를 잃은 상태면 유예 타이머
        if (bDetectLost)
        {
            DetectLostTimeAcc += DeltaTime;
            if (DetectLostTimeAcc >= DetectLossGraceTime)
            {
                const float Dist = FVector::Distance(GetActorLocation(), LastKnownTargetLocation);
                if (Dist > KeepChaseMaxDistance)
                {
                    bActive = false;
                    SetActorTickEnabled(false);
                    return;
                }
            }
        }
        else
        {
            bDetectLost = true;
            DetectLostTimeAcc = 0.f;
        }
    }

    // 현재 목표 위치
    const FVector TargetLoc = LastKnownTargetLocation;
    const FVector Current   = GetActorLocation();

    FVector Delta = TargetLoc - Current;
    const float Dist = Delta.Size();
    if (Dist <= KINDA_SMALL_NUMBER) return;

    if (Dist > StartDistanceToTarget)
        StartDistanceToTarget = Dist;

    // 속도 커브 적용
    UpdateSpeedByCurve(Dist);

    // 방향/스텝
    const FVector Dir  = Delta / Dist;
    const float   Step = CurrentSpeed * DeltaTime;

    // 이동(스윕 true로 경사면/충돌 반영)
    AddActorWorldOffset(Dir * Step, true);

    // 단차 보정 (지면 추적)
    UpdateHover(DeltaTime);

    // 디버그 드로우
    if (bDebugDraw)
    {
        DrawDebugLine(World, Current, TargetLoc, FColor::Cyan, false, DebugDrawDuration, 0, 1.5f);
        DrawDebugSphere(World, GetActorLocation(), 8.f, 8, FColor::Green,  false, DebugDrawDuration);
        DrawDebugSphere(World, TargetLoc,         12.f, 12, FColor::Magenta,false, DebugDrawDuration);
    }

    // CSV 로깅(부하를 줄이려면 간헐적으로만 호출 권장)
    if (bEnableCsvLogging)
    {
        const FString Line = FString::Printf(TEXT("Tick,%f,%s,%s,%.1f,%.1f"),
            UGameplayStatics::GetTimeSeconds(this),
            *GetName(),
            IsValid(T) ? *T->GetName() : TEXT("None"),
            Dist,
            CurrentSpeed);
        AppendCsv(Line);
    }
}

void ACHealOrb::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    UnbindOverlaps();

    if (UWorld* W = GetWorld())
    {
        W->GetTimerManager().ClearAllTimersForObject(this);
    }

    Super::EndPlay(EndPlayReason);
}

// ===== 내부 루틴 =====
void ACHealOrb::ResetOrbState()
{
    bActive   = false;
    bReleased = false;
    bDetectLost = false;
    DetectLostTimeAcc = 0.f;
    TargetActor = nullptr;
    LastKnownTargetLocation = FVector::ZeroVector;
    StartDistanceToTarget = 0.f;
    CurrentSpeed = 0.f;

    SetActorTickEnabled(false);
    SetActorHiddenInGame(false);

    if (UWorld* W = GetWorld())
    {
        W->GetTimerManager().ClearAllTimersForObject(this);
    }

    if (SpawnProjectile)
    {
        SpawnProjectile->StopMovementImmediately();
        SpawnProjectile->Deactivate();
    }
    
    if (PickupSphere)
        PickupSphere->SetSphereRadius(PickupRadius);
    if (DetectSphere)
        DetectSphere->SetSphereRadius(DetectRadius);

    bChaseAllowed = false;
    State = HealOrbState::Spawn;
    bSpawnPickupLocked = true;
}

void ACHealOrb::BindOverlaps()
{
    UnbindOverlaps(); // 중복 방지

    if (PickupSphere)
    {
        PickupSphere->OnComponentBeginOverlap.AddDynamic(this, &ACHealOrb::OnPickupOverlap);
    }
    if (DetectSphere)
    {
        DetectSphere->OnComponentBeginOverlap.AddDynamic(this, &ACHealOrb::OnDetectOverlap);
        DetectSphere->OnComponentEndOverlap.AddDynamic(this, &ACHealOrb::OnDetectEndOverlap);
    }
}

void ACHealOrb::UnbindOverlaps()
{
    if (PickupSphere)
        PickupSphere->OnComponentBeginOverlap.RemoveAll(this);
    if (DetectSphere)
    {
        DetectSphere->OnComponentBeginOverlap.RemoveAll(this);
        DetectSphere->OnComponentEndOverlap  .RemoveAll(this);
    }
}

void ACHealOrb::EnableCollisions(bool bEnable)
{
    const ECollisionEnabled::Type Mode = bEnable ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision;

    if (PickupSphere)
    {
        PickupSphere->SetCollisionEnabled(Mode);
        PickupSphere->SetGenerateOverlapEvents(bEnable);
    }
    if (DetectSphere)
    {
        DetectSphere->SetCollisionEnabled(Mode);
        DetectSphere->SetGenerateOverlapEvents(bEnable);
    }
}

void ACHealOrb::UpdateSpeedByCurve(float CurrentDist)
{
    // 0..1 진행도: 시작거리 기준으로 얼마나 근접했는가
    if (StartDistanceToTarget <= KINDA_SMALL_NUMBER)
    {
        // 시작거리가 아직 셋업 안 된 경우 현 거리로 초기화
        StartDistanceToTarget = CurrentDist;
    }

    const float Progress = FMath::Clamp(1.f - (CurrentDist / (StartDistanceToTarget + KINDA_SMALL_NUMBER)), 0.f, 1.f);

    float CurveVal = 1.f; // 기본 1배
    if (SpeedCurveAsset)
    {
        CurveVal = SpeedCurveAsset->GetFloatValue(Progress);
    }
    else if (const FRichCurve* RC = RuntimeSpeedCurve.GetRichCurveConst())
    {
        CurveVal = RC->Eval(Progress, 1.f);
    }

    CurveVal = FMath::Max(0.f, CurveVal) * CurveStrength;
    CurrentSpeed = BaseSpeed * CurveVal;
}

void ACHealOrb::UpdateHover(float DeltaTime)
{
    if (HoverHeight <= 0.f || HoverTraceLength <= 0.f) return;

    UWorld* World = GetWorld();
    if (!World) return;

    FVector Loc = GetActorLocation();
    
    FHitResult Hit;
    FVector Start = Loc + FVector(0,0,HoverTraceLength * 0.5f);
    FVector End   = Loc - FVector(0,0,HoverTraceLength);

    FCollisionQueryParams P(SCENE_QUERY_STAT(HealOrbHover), false, this);
    FCollisionResponseParams R;

    const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, P, R);
    if (bHit)
    {
        const float TargetZ = Hit.ImpactPoint.Z + HoverHeight;
        Loc.Z = FMath::FInterpTo(Loc.Z, TargetZ, DeltaTime, HoverLerpSpeed);
        SetActorLocation(Loc, false);
    }

    if (bDebugDraw)
    {
        DrawDebugLine(World, Start, End, FColor::Yellow, false, DebugDrawDuration, 0, 0.5f);
        if (bHit)
            DrawDebugSphere(World, Hit.ImpactPoint, 6.f, 8, FColor::Yellow, false, DebugDrawDuration);
    }
}

void ACHealOrb::AppendCsv(const FString& Line)
{
    if (!bEnableCsvLogging) return;

    const FString FullPath = FPaths::ProjectSavedDir() / CsvRelativePath;
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(FullPath), /*Tree*/true);

    const FString Row = Line + TEXT("\n");
    FFileHelper::SaveStringToFile(
        Row, *FullPath,
        FFileHelper::EEncodingOptions::AutoDetect,
        &IFileManager::Get(),
        FILEWRITE_Append
    );
}

void ACHealOrb::ApplyCurvePreset(EHealOrbSpeedCurvePreset Preset)
{
    FRichCurve* Curve = RuntimeSpeedCurve.GetRichCurve();
    if (!Curve) return;
    Curve->Reset();

    auto Key = [&](float X, float Y, ERichCurveInterpMode Mode = RCIM_Cubic)
    {
        FKeyHandle Handle = Curve->AddKey(X, Y);
        Curve->SetKeyInterpMode(Handle, Mode);
    };

    switch (Preset)
    {
    case EHealOrbSpeedCurvePreset::Linear:
        Key(0.f, 1.f, RCIM_Linear);
        Key(1.f, 1.f, RCIM_Linear);
        break;
    case EHealOrbSpeedCurvePreset::EaseIn:
        Key(0.f, 0.3f);
        Key(1.f, 1.f);
        break;
    case EHealOrbSpeedCurvePreset::EaseOut:
        Key(0.f, 1.5f);
        Key(1.f, 0.6f);
        break;
    case EHealOrbSpeedCurvePreset::EaseInOut:
        Key(0.f, 0.5f);
        Key(0.5f, 1.2f);
        Key(1.f, 0.8f);
        break;
    case EHealOrbSpeedCurvePreset::FastStartBrake:
        Key(0.f, 2.0f);   // 초반 빠르게
        Key(0.7f, 1.0f);  // 중반 안정
        Key(1.f, 0.5f);   // 끝에 브레이크
        break;
    case EHealOrbSpeedCurvePreset::RubberBand:
        Key(0.f, 2.2f);
        Key(0.4f, 1.0f);
        Key(0.8f, 1.3f);
        Key(1.f, 0.7f);
        break;
    }
}

bool ACHealOrb::HasLineOfSightToTarget(const FVector& From, const FVector& To) const
{
    FHitResult Hit;
    FCollisionQueryParams P(SCENE_QUERY_STAT(HealOrbLOS), false, this);
    const bool bBlocking = GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, P);
    if (!bBlocking) return true;
    
    // 타깃 자신이 히트면 LOS로 간주
    return Hit.GetActor() == TargetActor.Get();
}

void ACHealOrb::EnterSpawnState()
{
    State = HealOrbState::Spawn;
    bChaseAllowed = false;
    bSpawnPickupLocked = true;
    if (ensureMsgf(PickupSphere != nullptr, TEXT("HealOrb missing PickupSphere component")))
    {
        PickupSphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
        PickupSphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
        PickupSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
        PickupSphere->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);
        PickupSphere->BodyInstance.bUseCCD = true;
    }
    const FVector Rand2D = FVector(FMath::FRandRange(-1.0f,1.0f), FMath::FRandRange(-1.0f,1.0f), 0.0f).GetSafeNormal();

    if (SpawnProjectile)
    {
        SpawnProjectile->Velocity = Rand2D * SpawnHorizontalSpeed + FVector::UpVector * SpawnUpSpeed;
        SpawnProjectile->Activate(true);
    }
}

void ACHealOrb::EnterChaseState()
{
    bChaseAllowed = true;
    State = HealOrbState::Chase;

    if (SpawnProjectile && SpawnProjectile->IsActive())
    {
        SpawnProjectile->StopMovementImmediately();
        SpawnProjectile->Deactivate();
    }
    if (ensureMsgf(PickupSphere != nullptr, TEXT("HealOrb missing PickupSphere component")))
    {
        PickupSphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Overlap);
        PickupSphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
        PickupSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
        PickupSphere->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Overlap);
        PickupSphere->BodyInstance.bUseCCD = true;
    }
    if (AActor* T = TargetActor.Get())
    {
        LastKnownTargetLocation = T->GetActorLocation();
        StartDistanceToTarget = FVector::Distance(GetActorLocation(), LastKnownTargetLocation);
    }
}

void ACHealOrb::ActivateOrb(AActor* PreferredTarget)
{
    ResetOrbState();
    EnterSpawnState();
    BindOverlaps();
    EnableCollisions(true);

    if (PreferredTarget)
        TargetActor = PreferredTarget;

    if (AActor* T = TargetActor.Get())
    {
        StartDistanceToTarget   = FVector::Distance(GetActorLocation(), T->GetActorLocation());
        LastKnownTargetLocation = T->GetActorLocation();

        if (bEnableCsvLogging)
        {
            const FString Line = FString::Printf(TEXT("Spawn,%f,%s,%s,%.1f,%.1f"),
                UGameplayStatics::GetTimeSeconds(this),
                *GetName(),
                *T->GetName(),
                StartDistanceToTarget,
                CurrentSpeed);
            AppendCsv(Line);
        }
    }

    bActive = true;
    SetActorTickEnabled(true);
    
    if (UWorld* W = GetWorld())
    {
        W->GetTimerManager().SetTimer(SpawnDelayHandle, this, &ACHealOrb::EnterChaseState, 1.0f, false);
    }

    // VFX/SFX
    if (VFX_Spawn)
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, VFX_Spawn, GetActorLocation());
    if (SFX_Spawn)
        UGameplayStatics::PlaySoundAtLocation(this, SFX_Spawn, GetActorLocation());

    OnOrbSpawned.Broadcast(this);
}

void ACHealOrb::ReleaseOrb(bool bReturnToPool)
{
    if (bReleased) return;
    bReleased = true;

    bActive = false;
    SetActorTickEnabled(false);

    UnbindOverlaps();
    EnableCollisions(false);

    if (UWorld* W = GetWorld())
    {
        W->GetTimerManager().ClearAllTimersForObject(this);
    }

    if (SpawnProjectile)
    {
        SpawnProjectile->StopMovementImmediately();
        SpawnProjectile->Deactivate();
    }
    
    SetActorHiddenInGame(true);
    TargetActor = nullptr;

    if (!bReturnToPool)
    {
        // 파괴 경로(풀 미사용/서브시스템 없음)
        if (UWorld* W = GetWorld())
        {
            W->GetTimerManager().SetTimerForNextTick([this]()
            {
                if (IsValid(this))
                    Destroy();
            });
        }
    }
    else
    {
        // ★중요★: 풀 반납은 "서브시스템"이 호출(Release)합니다.
        // 여기서 다시 풀에 통지하면 재귀가 됩니다. (아무 것도 하지 않음)
    }
}

void ACHealOrb::SetTarget(AActor* NewTarget)
{
    if (!IsValid(NewTarget)) return;

    TargetActor = NewTarget;
    LastKnownTargetLocation = NewTarget->GetActorLocation();
    StartDistanceToTarget   = FVector::Distance(GetActorLocation(), LastKnownTargetLocation);

    bDetectLost = false;
    DetectLostTimeAcc = 0.f;

    bActive = true;
    SetActorTickEnabled(true);
}


// ===== 오버랩 핸들러 =====
void ACHealOrb::OnPickupOverlap(UPrimitiveComponent* Overlapped, AActor* OtherActor,
                                UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                                bool bFromSweep, const FHitResult& Hit)
{
    if (bReleased) return;
    if (!IsValid(OtherActor) || OtherActor == this) return;
    if (State == HealOrbState::Spawn && bSpawnPickupLocked) return;

    if (UCPlayerHealthComponent* Health = OtherActor->FindComponentByClass<UCPlayerHealthComponent>())
    {
        // 치유
        Health->Healing(HealAmount);

        // VFX/SFX
        if (VFX_Pick)
            UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, VFX_Pick, GetActorLocation());
        if (SFX_Pick)
            UGameplayStatics::PlaySoundAtLocation(this, SFX_Pick, GetActorLocation());

        // 이벤트
        OnOrbPicked.Broadcast(this, OtherActor);

        // CSV
        if (bEnableCsvLogging)
        {
            const float Dist = FVector::Distance(GetActorLocation(), OtherActor->GetActorLocation());
            const FString Line = FString::Printf(TEXT("Pick,%f,%s,%s,%.1f,%.1f"),
                UGameplayStatics::GetTimeSeconds(this),
                *GetName(),
                *OtherActor->GetName(),
                Dist,
                CurrentSpeed);
            AppendCsv(Line);
        }

        // 풀 사용 시: 서브시스템 경유로 반납
        // 서브시스템이 없으면(=싱글/테스트) 로컬 해제
        if (bUsePooling)
        {
            if (UGameInstance* GI = GetGameInstance())
            {
                if (auto* Pool = GI->GetSubsystem<class UCHealOrbPoolSubsystem>())
                {
                    Pool->NotifyPicked(this); // 내부에서 Release(Orb) → Orb->ReleaseOrb(true)
                }
                else
                {
                    ReleaseOrb(true); // 풀 없으면 로컬 비활성화만
                }
            }
            else
            {
                ReleaseOrb(true);
            }
        }
        else
        {
            ReleaseOrb(false); // 파괴 경로
        }
    }
}

void ACHealOrb::OnDetectOverlap(UPrimitiveComponent* Overlapped, AActor* OtherActor,
                                UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                                bool bFromSweep, const FHitResult& Hit)
{
    if (bReleased) return;
    if (!IsValid(OtherActor) || OtherActor == this) return;

    if (OtherActor->FindComponentByClass<UCPlayerHealthComponent>())
    {
        if (!bChaseAllowed)
        {
            TargetActor = OtherActor;
            return;
        }

        SetTarget(OtherActor); // 타깃 지정 및 추적 시작
    }
}

void ACHealOrb::OnDetectEndOverlap(UPrimitiveComponent* Overlapped, AActor* OtherActor,
                                   UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (bReleased) return;
    if (!IsValid(OtherActor)) return;

    if (TargetActor.IsValid() && OtherActor == TargetActor.Get())
    {
        // Detect 범위를 이탈했지만, 유예시간/거리 정책으로 계속 추적할 수 있음
        bDetectLost = true;
        DetectLostTimeAcc = 0.f;
    }
}

void ACHealOrb::OnSpawnProjectileStop(const FHitResult& ImpactResult)
{
    if (ensureMsgf(PickupSphere != nullptr, TEXT("HealOrb missing PickupSphere component")))
    {
        PickupSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
        PickupSphere->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Overlap);
    }
    if (SpawnProjectile)
    {
        SpawnProjectile->StopMovementImmediately();
        SpawnProjectile->Deactivate();
    }
}
