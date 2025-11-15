#include "CRangedSkirmisher.h"

#include "01_AIController/CTacticalEnemyAIController.h"
#include "00_Character/00_Player/CPlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "NiagaraFunctionLibrary.h"
#include "05_System/01_Sound/CSoundManagerSubsystem.h"
#include "05_System/01_Sound//CSoundDataAsset.h"
#include "00_Character/CMainGameModeBase.h"

// ─────────────────────────────────────────────────────
ACRangedSkirmisher::ACRangedSkirmisher()
{
    AIControllerClass = ACTacticalEnemyAIController::StaticClass();
    AutoPossessAI     = EAutoPossessAI::PlacedInWorldOrSpawned;

    // 베이스 튜닝(근접값은 무시되고 사격으로 전투)
    AttackInterval = BurstShotInterval;      // EnemyBase의 공통 타이머와 맞춤
    BaseDamage     = BulletDamage;

    // 중요: 원거리 적이므로 AttackRange를 사격 거리로 설정
    AttackRange = 900.f;  // DesiredRangeMax보다 약간 크게
    AttackEnterDistance = 900.f;  // Attack 상태 진입 거리
    AttackExitDistance = 1000.f;  // Attack 상태 이탈 거리
    
    UE_LOG(LogTemp, Warning, TEXT("[Skirmisher] 생성자: AttackRange = %.2f로 설정"), AttackRange);

    // 이동 빠름
    if (UCharacterMovementComponent* M = GetCharacterMovement())
    {
        M->MaxWalkSpeed = RunSpeed;
        M->bUseRVOAvoidance = true;
        M->AvoidanceConsiderationRadius = 800.f;
    }
}

void ACRangedSkirmisher::BeginPlay()
{
    Super::BeginPlay();
    if (USkeletalMeshComponent* mesh = GetMesh())
    {
        InitialMeshRelativeTransform = mesh->GetRelativeTransform();
    }
    // 피해 이벤트 → 반사 회피 트리거
    OnTakeAnyDamage.AddDynamic(this, &ACRangedSkirmisher::OnAnyDamaged);
    
    UE_LOG(LogTemp, Warning, TEXT("[Skirmisher] BeginPlay 완료 - AttackRange: %.2f"), AttackRange);
}

void ACRangedSkirmisher::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    
    // 현재 상태 주기적 로그 (1초마다)
    /*static float LogTimer = 0.f;
    LogTimer += DeltaSeconds;
    if (LogTimer >= 1.0f)
    {
        LogTimer = 0.f;
        if (Target)
        {
            const float D = DistToTarget();
            const FString StateName = UEnum::GetValueAsString(State);
            UE_LOG(LogTemp, Log, TEXT("[Skirmisher] 현재 상태: %s, 타겟 거리: %.2f, AttackRange: %.2f"), 
                   *StateName, D, AttackRange);
        }
    }*/
}


void ACRangedSkirmisher::OnDead()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(BurstTimerHandle);
    }
    ShotsFiredInBurst = 0;
    LastBurstTime = -1000.f;

    PlayMontageIfValid(DeadMontage);
    
    OnTakeAnyDamage.RemoveDynamic(this, &ACRangedSkirmisher::OnAnyDamaged);
    Super::OnDead();
    
    if (USkeletalMeshComponent* mesh = GetMesh())
    {
        mesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
        mesh->SetSimulatePhysics(true);
        
        mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        mesh->SetCollisionProfileName(TEXT("Ragdoll"));
        
        FVector ImpulseDir = FVector(
            FMath::FRandRange(-100.f, 100.f),  // 좌우 랜덤
            FMath::FRandRange(-100.f, 100.f),  // 앞뒤 랜덤
            FMath::FRandRange(200.f, 400.f)    // 위로 튀기
        );
        mesh->AddImpulse(ImpulseDir, NAME_None, true);
    }
    
    if (UCapsuleComponent* Capsule = GetCapsuleComponent())
    {
        Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
}
void ACRangedSkirmisher::OnResetForRespawn_Implementation()
{
    Super::OnResetForRespawn_Implementation();
    
    ShotsFiredInBurst = 0;
    LastBurstTime = -1000.f;
    
    if (USkeletalMeshComponent* mesh = GetMesh())
    {
        mesh->SetSimulatePhysics(false);
        mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        mesh->SetCollisionProfileName(TEXT("CharacterMesh"));
        mesh->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::KeepRelativeTransform);
        mesh->SetRelativeTransform(InitialMeshRelativeTransform);
    }
    
    if (UCapsuleComponent* Capsule = GetCapsuleComponent())
    {
        Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    }
    
    if (!OnTakeAnyDamage.IsAlreadyBound(this, &ACRangedSkirmisher::OnAnyDamaged))
    {
        OnTakeAnyDamage.AddDynamic(this, &ACRangedSkirmisher::OnAnyDamaged);
    }
}


// ───────────────── FSM 확장 ─────────────────
void ACRangedSkirmisher::DoChase()
{
    if (!Target)
    {
        SetState(EEnemyState::ReturnHome);
        return;
    }

    const float D = DistToTarget();

    if (D >= ChaseStopDistance || (!HasVisualOnTarget() && GetWorld()->GetTimeSeconds() - LastSeenTime >= LoseSightGrace))
    {
        SetState(EEnemyState::ReturnHome);
        return;
    }
    
    if (HasVisualOnTarget())
    {
        LastSeenTime = GetWorld()->GetTimeSeconds();
    }
    
    // 사격 거리 안에 들어오면 Attack 상태로 전환
    if (D <= AttackEnterDistance)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Skirmisher] DoChase: 사격 거리 안! Attack 상태로 전환"));
        SetState(EEnemyState::Attack);
        return;
    }

    // 사거리 밴드 유지
    if (bUseNavigation)
    {
        if (ACTacticalEnemyAIController* TAC = Cast<ACTacticalEnemyAIController>(GetController()))
        {
            // 너무 가까우면 살짝 후퇴(타깃 반대 방향으로 이동)
            if (D < DesiredRangeMin - 80.f)
            {
                const FVector AwayDir = (GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D();
                const FVector RetreatPoint = GetActorLocation() + AwayDir * 400.f;
                TAC->TacticalRetreatTo(RetreatPoint, 120.f);
                return;
            }

            // 너무 멀면 링 포위 반경으로 접근
            if (D > DesiredRangeMax + 80.f)
            {
                TAC->TacticalChaseRing(Target, /*radius=*/(DesiredRangeMin + DesiredRangeMax)*0.5f,
                                       /*acc=*/PatrolPointReachRadius, /*focus=*/true);
                return;
            }

            // 적정 거리이면 좌/우 스트레이프
            const float Sign = (FMath::FRand() > 0.5f) ? +1.f : -1.f;
            TAC->TacticalStrafe(Target, /*radius=*/(DesiredRangeMin + DesiredRangeMax)*0.5f,
                                /*deg/s=*/Sign*140.f, /*dur=*/0.6f, /*acc=*/110.f);
            return;
        }
    }

    // NavMesh 없는 맵: 기본 직진 이동으로 근접/이탈
    const FVector Goal = (D < DesiredRangeMin) ? 
        GetActorLocation() + (GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D()*300.f :
        Target->GetActorLocation();

    RequestMoveTo(Goal, PatrolPointReachRadius);
}

void ACRangedSkirmisher::DoAttack()
{
    if (!Target)
    {
        SetState(EEnemyState::Chase);
        return;
    }

    const float D = DistToTarget();

    // 너무 멀어지면 다시 Chase로
    if (D > AttackExitDistance)
    {
        SetState(EEnemyState::Chase);
        return;
    }

    // 회피(확률적)
    TryEvadeRandom();

    // 사격 조건: 거리 밴드 + LoS + 쿨타임
    const float Now = GetWorld()->GetTimeSeconds();
    const bool bRangeOK = (D >= DesiredRangeMin && D <= DesiredRangeMax);
    const bool bHasClearShot = HasClearShot();
    const bool bTimerActive = GetWorldTimerManager().IsTimerActive(BurstTimerHandle);
    const float TimeSinceLastBurst = Now - LastBurstTime;
    const bool bCooldownReady = (TimeSinceLastBurst >= BurstCooldown);
    const bool bBurstReady = (!bTimerActive && bCooldownReady);
    
   
    
    if (bRangeOK && bHasClearShot && bBurstReady)
    {
        StartBurst();

        // 버스트 동안 스트레이프로 움직임 유지       
        if (ACTacticalEnemyAIController* TAC = Cast<ACTacticalEnemyAIController>(GetController()))
        {
            const float Sign = (FMath::FRand() > 0.5f) ? +1.f : -1.f;
            TAC->TacticalStrafe(Target, (DesiredRangeMin + DesiredRangeMax)*0.5f,
                                Sign*160.f, 0.5f, 100.f);
        }
        return;
    }

    // 너무 가까우면 추적으로 반환하여 거리 벌림
    if (D < DesiredRangeMin - 60.f)
    {
        SetState(EEnemyState::Chase);
    }
}

void ACRangedSkirmisher::ExitState(EEnemyState OldState)
{
    Super::ExitState(OldState);
    if (GetWorldTimerManager().IsTimerActive(BurstTimerHandle))
    {
        GetWorldTimerManager().ClearTimer(BurstTimerHandle);
        if (UWorld* World = GetWorld())
        {
            LastBurstTime = World->GetTimeSeconds();
        }
    }
}

// ───────────────── 사격 ─────────────────
void ACRangedSkirmisher::StartBurst()
{
    UE_LOG(LogTemp, Warning, TEXT("[Skirmisher] ========== StartBurst 호출됨 =========="));
    UE_LOG(LogTemp, Warning, TEXT("[Skirmisher] ShotsPerBurst = %d, BurstShotInterval = %.2f"), ShotsPerBurst, BurstShotInterval);
    
    ShotsFiredInBurst = 0;
    FireBurstShot();

    if (ShotsPerBurst <= 1) 
    {
        UE_LOG(LogTemp, Warning, TEXT("[Skirmisher] ShotsPerBurst가 1이하라서 추가 타이머 없이 종료"));
        return;
    }

    const float Interval = FMath::Max(0.f, BurstShotInterval);
    if (Interval <= 0.f)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Skirmisher] Interval이 0이라서 즉시 모든 샷 발사"));
        while (ShotsFiredInBurst < ShotsPerBurst)
        {
            FireBurstShot();
        }
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("[Skirmisher] %.2f초 간격으로 타이머 설정"), Interval);
    GetWorldTimerManager().SetTimer(BurstTimerHandle, this, &ACRangedSkirmisher::FireBurstShot, Interval, true);
}

void ACRangedSkirmisher::FireBurstShot()
{
    UE_LOG(LogTemp, Warning, TEXT("[Skirmisher] ========== FireBurstShot 호출 (%d / %d) =========="), 
           ShotsFiredInBurst + 1, ShotsPerBurst);

    if (!Target || ShotsFiredInBurst >= ShotsPerBurst)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Skirmisher] FireBurstShot: 버스트 종료 (Target=%s, Shots=%d/%d)"),
               Target ? TEXT("Valid") : TEXT("Null"), ShotsFiredInBurst, ShotsPerBurst);
        GetWorldTimerManager().ClearTimer(BurstTimerHandle);
        if (UWorld* World = GetWorld())
        {
            LastBurstTime = World->GetTimeSeconds();
        }
        return;
    }
    
    FireOnce();
    ++ShotsFiredInBurst;
    
    if (ShotsFiredInBurst >= ShotsPerBurst)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Skirmisher] FireBurstShot: 최종 샷 발사 완료, 타이머 정리"));
        GetWorldTimerManager().ClearTimer(BurstTimerHandle);
        if (UWorld* World = GetWorld())
        {
            LastBurstTime = World->GetTimeSeconds();
        }
    }
}

// ───────────────── 사격 ─────────────────
void ACRangedSkirmisher::FireOnce()
{
    UE_LOG(LogTemp, Error, TEXT("[Skirmisher] !!!! FireOnce 호출됨 !!!! "));

    if (!ProjectileClass)
    {
        UE_LOG(LogTemp, Error, TEXT("[Skirmisher] ProjectileClass가 nullptr입니다! BP에서 설정했는지 확인하세요."));
        return;
    }

    if (!Target)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Skirmisher] Target이 nullptr입니다."));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("[Skirmisher] ProjectileClass: %s"), *ProjectileClass->GetName());

    FRotator MuzzleRot;
    const FVector MuzzleLoc = GetMuzzleLocation(MuzzleRot);

    // 리드샷 방향
    FVector TargetVel = FVector::ZeroVector;
    if (const ACharacter* PC = Cast<ACharacter>(Target))
        if (const UCharacterMovementComponent* MV = PC->GetCharacterMovement())
            TargetVel = MV->Velocity;

    FVector AimDir = ComputeLeadAimDir(MuzzleLoc, Target->GetActorLocation(), TargetVel, ProjectileSpeed);
    if (!AimDir.Normalize())
        AimDir = GetActorForwardVector();

    // 약간의 퍼짐
    const FRotator SpreadRot( FMath::FRandRange(-SpreadDegrees, +SpreadDegrees),
                              FMath::FRandRange(-SpreadDegrees, +SpreadDegrees),
                              0.f );
    AimDir = SpreadRot.RotateVector(AimDir).GetSafeNormal();

    FTransform SpawnTM(FRotationMatrix::MakeFromX(AimDir).Rotator(), MuzzleLoc);

    UE_LOG(LogTemp, Warning, TEXT("[Skirmisher] 발사체 스폰 시도 위치: %s"), *MuzzleLoc.ToString());

    FActorSpawnParameters P;
    P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    P.Owner = this;
    P.Instigator = this;

    AActor* Spawned = GetWorld()->SpawnActor<AActor>(ProjectileClass, SpawnTM, P);
    
    if (Spawned)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Skirmisher]  발사체 스폰 성공! -> %s (위치: %s)"), 
               *Spawned->GetName(), *Spawned->GetActorLocation().ToString());

        // 탄환 초기화(속도/데미지/슈터 지정) — CEnemyBullet에 구현
        if (UFunction* InitFn = Spawned->FindFunction(TEXT("InitBullet")))
        {
            UE_LOG(LogTemp, Warning, TEXT("[Skirmisher] InitBullet 함수 찾음, 호출합니다."));
            struct FInitParams { AActor* Shooter; float Damage; float Speed; FVector Dir; };
            FInitParams Params{ this, BulletDamage, ProjectileSpeed, AimDir };
            Spawned->ProcessEvent(InitFn, &Params);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("[Skirmisher] InitBullet 함수를 찾을 수 없습니다!"));
        }

        if (MuzzleFX)
            UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, MuzzleFX, MuzzleLoc, GetActorRotation());
        //if (FireSFX)
          //  UGameplayStatics::PlaySoundAtLocation(this, FireSFX, MuzzleLoc);
        if (FireMontage)
            if (UAnimInstance* Anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
                Anim->Montage_Play(FireMontage, 1.0f);

        FVector MuzzlePos = GetMuzzleLocation(MuzzleRot);
    
        if (UGameInstance* GI = GetGameInstance())
        {
            if (UCSoundManagerSubsystem* SoundMgr = GI->GetSubsystem<UCSoundManagerSubsystem>())
            {
                SoundMgr->PlaySFX3D(CachedAttackSound.Get(), MuzzlePos, 0.7f);
            }
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[Skirmisher] 발사체 스폰 실패! ProjectileClass가 제대로 설정되었는지 확인하세요."));
    }
}

FVector ACRangedSkirmisher::ComputeLeadAimDir(const FVector& From, const FVector& TargetPos, const FVector& TargetVel, float ProjSpeed) const
{
    // From + v*t = TargetPos + TargetVel*t   에서  |v|=ProjSpeed
    // t를 풀어 근사. 속도가 너무 느리거나 해가 없으면 현재 위치 조준
    const FVector R = TargetPos - From;
    const float  a = TargetVel.SizeSquared() - ProjSpeed*ProjSpeed;
    const float  b = 2.f * FVector::DotProduct(R, TargetVel);
    const float  c = R.SizeSquared();

    float t = 0.f;
    if (FMath::Abs(a) < KINDA_SMALL_NUMBER)
    {
        // 직선 근사
        t = (c > 0.f) ? ( -c / b ) : 0.f;
    }
    else
    {
        const float D = b*b - 4*a*c;
        if (D < 0.f) return (TargetPos - From).GetSafeNormal();
        const float SqrtD = FMath::Sqrt(D);
        const float t1 = (-b + SqrtD) / (2*a);
        const float t2 = (-b - SqrtD) / (2*a);
        t = (t1 > 0.f && t2 > 0.f) ? FMath::Min(t1, t2) : FMath::Max(t1, t2);
        if (t <= 0.f) return (TargetPos - From).GetSafeNormal();
    }
    return (R + TargetVel * t).GetSafeNormal();
}

FVector ACRangedSkirmisher::GetMuzzleLocation(FRotator& OutMuzzleRot) const
{
    if (const USkeletalMeshComponent* mesh = GetMesh())
    {
        if (mesh->DoesSocketExist(MuzzleSocket))
        {
            const FTransform T = mesh->GetSocketTransform(MuzzleSocket);
            OutMuzzleRot = T.Rotator();
            return T.GetLocation();
        }
    }
    OutMuzzleRot = GetActorRotation();
    return GetActorLocation() + MuzzleOffset;
}

// ───────────────── 회피 ─────────────────
void ACRangedSkirmisher::TryEvadeRandom()
{
    if (!bCanEvade || !Target) return;

    const float Now = GetWorld()->GetTimeSeconds();
    if (Now - LastEvadeTime < EvadeCooldown) return;

    const float D = DistToTarget();
    if (D > EvadeTriggerDistance) return;

    float AdjustedChance = EvadeChanceOnThink;
    if (EvadeTriggerDistance > KINDA_SMALL_NUMBER)
    {
        const float DistanceAlpha = FMath::Clamp(D / EvadeTriggerDistance, 0.f, 1.f);
        const float ChanceScale = FMath::Lerp(1.f, EvadeChanceAtMaxDistanceScale, DistanceAlpha);
        AdjustedChance *= ChanceScale;
    }

    if (FMath::FRand() < AdjustedChance)
    {
        DoEvade(/*preferLeft=*/FMath::FRand() > 0.5f);
        LastEvadeTime = Now;
    }
}

void ACRangedSkirmisher::DoEvade(bool bPreferLeft)
{
    if (UCharacterMovementComponent* M = GetCharacterMovement())
    {
        const FVector ToTarget = (Target ? (Target->GetActorLocation() - GetActorLocation()) : GetActorForwardVector());
        FVector Right = FVector::CrossProduct(FVector::UpVector, ToTarget.GetSafeNormal2D());
        if (!bPreferLeft) Right *= -1.f;

        const FVector Impulse = Right.GetSafeNormal() * EvadeImpulse + FVector(0,0, 150.f);
        LaunchCharacter(Impulse, true, true);
    }
}

void ACRangedSkirmisher::OnAnyDamaged(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
                                      AController* InstigatedBy, AActor* DamageCauser)
{
    if (!bCanEvade || !Target) return;
    const float Now = GetWorld()->GetTimeSeconds();
    if (Now - LastEvadeTime < EvadeCooldown) return;

    // 맞으면 즉시 회피
    DoEvade(/*preferLeft=*/FMath::FRand() > 0.5f);
    LastEvadeTime = Now;
}

// ───────────────── 유틸 ─────────────────
bool ACRangedSkirmisher::HasClearShot() const
{
    if (!Target) return false;
    FHitResult Hit;
    const FVector S = GetActorLocation() + FVector(0,0, 60.f);
    const FVector E = Target->GetActorLocation() + FVector(0,0, 50.f);
    FCollisionQueryParams Q(SCENE_QUERY_STAT(PF_Ranged_Shot), false, this);
    const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, S, E, SightTraceChannel, Q);
    return (!bHit || Hit.GetActor() == Target);
}

// ─────────────────사운드────────────────────────
void ACRangedSkirmisher::CacheSoundsFromDataAsset()
{
    if (ACMainGameModeBase* GM = Cast<ACMainGameModeBase>(GetWorld()->GetAuthGameMode()))
    {
        if (UCSoundDataAsset* SoundData = GM->GetSoundDataAsset())
        {
            const FCharacterSoundCollection* Sounds = SoundData->GetCharacterSounds(TEXT("RangedSkirmisher"));
            if (Sounds)
            {
                CachedAttackSound = Sounds->AttackSound; // 발사 사운드
                CachedHitSound = Sounds->HitSound;
                CachedDeathSound = Sounds->DeathSound;
            }
        }
    }
}