#include "CRangedSkirmisher.h"

#include "01_AIController/CTacticalEnemyAIController.h"
#include "00_Character/00_Player/CPlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

// ─────────────────────────────────────────────────────
ACRangedSkirmisher::ACRangedSkirmisher()
{
    AIControllerClass = ACTacticalEnemyAIController::StaticClass();
    AutoPossessAI     = EAutoPossessAI::PlacedInWorldOrSpawned;

    // 베이스 튜닝(근접값은 무시되고 사격으로 전투)
    AttackInterval = FireInterval;        // EnemyBase의 공통 타이머와 맞춤
    BaseDamage     = BulletDamage;

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

    // 피해 이벤트 → 반사 회피 트리거
    OnTakeAnyDamage.AddDynamic(this, &ACRangedSkirmisher::OnAnyDamaged);
}

void ACRangedSkirmisher::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
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

    // 회피(확률적)
    TryEvadeRandom();

    // 사격 조건: 거리 밴드 + LoS + 쿨타임
    const float Now = GetWorld()->GetTimeSeconds();
    const bool bRangeOK = (D >= DesiredRangeMin && D <= DesiredRangeMax);
    if (bRangeOK && HasClearShot() && Now - LocalLastFireTime >= FireInterval)
    {
        FireOnce();
        LocalLastFireTime = Now;

        // 쿨다운 동안 스트레이프
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
    // 사격은 타이머를 쓰지 않으므로 특별 정리 없음
}

// ───────────────── 사격 ─────────────────
void ACRangedSkirmisher::FireOnce()
{
    if (!ProjectileClass || !Target) return;

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

    FActorSpawnParameters P;
    P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    P.Owner = this;
    P.Instigator = this;

    AActor* Spawned = GetWorld()->SpawnActor<AActor>(ProjectileClass, SpawnTM, P);
    if (Spawned)
    {
        // 탄환 초기화(속도/데미지/슈터 지정) — CEnemyBullet에 구현
        if (UFunction* InitFn = Spawned->FindFunction(TEXT("InitBullet")))
        {
            struct FInitParams { AActor* Shooter; float Damage; float Speed; FVector Dir; };
            FInitParams Params{ this, BulletDamage, ProjectileSpeed, AimDir };
            Spawned->ProcessEvent(InitFn, &Params);
        }

        if (MuzzleFX)
            UGameplayStatics::SpawnEmitterAtLocation(this, MuzzleFX, MuzzleLoc, AimDir.Rotation());
        if (FireSFX)
            UGameplayStatics::PlaySoundAtLocation(this, FireSFX, MuzzleLoc);
        if (FireMontage)
            if (UAnimInstance* Anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
                Anim->Montage_Play(FireMontage, 1.0f);
    }

    if (bDebugLog)
        UE_LOG(LogTemp, Verbose, TEXT("[Skirmisher] FireOnce -> %s"), *Spawned->GetName());
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

    if (FMath::FRand() < EvadeChanceOnThink)
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
