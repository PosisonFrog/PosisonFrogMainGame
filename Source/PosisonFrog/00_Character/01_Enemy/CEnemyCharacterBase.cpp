#include "CEnemyCharacterBase.h"

#include "AIController.h"
#include "NavigationSystem.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "Navigation/PathFollowingComponent.h"

#include "00_Character/00_Player/CPlayerCharacter.h"
#include "00_Character/02_Component/01_EnemyComponent/CEnemyHealthComponent.h"
#include "01_Item/CHealOrbPoolSubsystem.h"

#include "99_Util/CLog.h"

ACEnemyCharacterBase::ACEnemyCharacterBase()
{
	PrimaryActorTick.bCanEverTick = true;

	HealthComponent = CreateDefaultSubobject<UCEnemyHealthComponent>(TEXT("HealthComponent"));

	if (UCapsuleComponent* Cap = GetCapsuleComponent())
	{
		// RVOAvoidanceRadius 변수 값으로 캡슐 반지름 설정 후 생성(아래 RVO 회피반경 주석 이걸로 바꿈.)
		Cap->SetCapsuleRadius(RVOAvoidanceRadius);
	}
	
	if (UCharacterMovementComponent* M = GetCharacterMovement())
	{
		M->MaxWalkSpeed = 360.0f;
		M->bUseControllerDesiredRotation = false;
		M->bOrientRotationToMovement = true;
		M->RotationRate = FRotator(0.f, 420.f, 0.f);

		// RVO 회피 보조
		if (bUseRVOAvoidance)
		{
			M->bUseRVOAvoidance = true;
			M->AvoidanceConsiderationRadius = RVOConsiderationRadius;
			M->AvoidanceWeight = RVOAvoidanceWeight;


			//UCharacterMovementComponent에 RVOAvoidanceRadius가 없더라...
			//M->RVOAvoidanceRadius = RVOAvoidanceRadius;
		}
	}

	LastAttackSweepTime = 0.f;

	// 개체별 포위 각도 시드(재현성 있는 난수임)
	const uint32 Hash = ::GetTypeHash(this); //각 AI의 고유한 ID

	//고유한 기본 방향을 Hash % 360도로 나눈 값으로 할당해서 서로 겹치지 않게 하고, 거기에 약간의 무작위성 추가.
	MyChaseAngleDeg = FMath::Fmod(
		(float)(Hash % 360u) + FMath::FRandRange(-ChaseRingAngleJitterDeg, +ChaseRingAngleJitterDeg),
		360.0f
		);
}

void ACEnemyCharacterBase::BeginPlay()
{
	Super::BeginPlay();
	
	HomeLocation = GetActorLocation();

	// 직진 이동
	if (!bUseNavigation)
	{
		if (UCharacterMovementComponent* M = GetCharacterMovement())
			M->MaxWalkSpeed = DirectMoveSpeed;
	}
	
	if (HealthComponent)
	{
		HealthComponent->OnHealthChanged.AddDynamic(this, &ACEnemyCharacterBase::OnHealthChanged);
	}

	SetState(EEnemyState::Patrol); // 기본적으로 순찰모드
}

void ACEnemyCharacterBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	
	// 스윙 창: 프레임 독립 분할 스윕
	if (bAttackWindowActive)
	{
		PerformAttackSweep();

		if (AttackWindowEndTime > 0.f && GetWorld()->GetTimeSeconds() >= AttackWindowEndTime)
		{
			AttackWindowEnd(/*bForce=*/true);
		}
	}

	// 직진 스티어링 이동 처리(분리 포함)
	if (!bUseNavigation && bDirectMoveActive)
		DirectMoveTick(DeltaSeconds);

	// Think 빈도 가변 : 플레이어가 멀리 떨어져있다면 저비용 생각 주기, 가까이 있다면 고비용 생각 주기로 반응 가속.
	const float Dist = Target ? FVector::Dist(GetActorLocation(), Target->GetActorLocation()) : FLT_MAX;
	const float Interval = (Dist <= NearThinkDistance) ? RichThinkInterval : CheapThinkInterval;
	
	if (GetWorld()->GetTimeSeconds() >= NextThinkTime)
	{
		NextThinkTime = GetWorld()->GetTimeSeconds() + Interval;
		Think(DeltaSeconds);

		if (bShowDebugInfo)
			DebugDrawState();
	}

}



// ─────────────────────────────────────────────────────────────────────────────
// FSM
// ─────────────────────────────────────────────────────────────────────────────
void ACEnemyCharacterBase::Think(float /*DeltaTime*/)
{
	AcquireTarget();

	switch (State)
	{
	case EEnemyState::Patrol:     DoPatrol();      break;
	case EEnemyState::Alert:      DoAlert();       break;
	case EEnemyState::Chase:      DoChase();       break;
	case EEnemyState::Attack:     DoAttack();      break;
	case EEnemyState::ReturnHome: DoReturnHome();  break;
	case EEnemyState::Dead:       DoDead();        break;
	}
}


void ACEnemyCharacterBase::EnterState(EEnemyState /*NewState*/)
{
	StateEnterTime = GetWorld()->GetTimeSeconds();
}

void ACEnemyCharacterBase::ExitState(EEnemyState /*OldState*/)
{
}

void ACEnemyCharacterBase::SetState(EEnemyState NewState)
{
	if (State == NewState)
		return;

	ExitState(State);
	State = NewState;
	EnterState(State);
}



// ─────────────────────────────────────────────────────────────────────────────
// 상태 처리
// ─────────────────────────────────────────────────────────────────────────────
void ACEnemyCharacterBase::DoPatrol()
{
	// Patrol → Alert : LoS && Dist ≤ ChaseStartDistance
	if (Target && HasVisualOnTarget() && DistToTarget() <= ChaseStartDistance)
	{
		SetState(EEnemyState::Alert);
		return;
	}

	// 현재 순찰 도착 위치가 0에 가깝거나 AI가 목표지점에 도달 했으면
	if (PatrolGoal.IsNearlyZero() || Reached(PatrolGoal, PatrolPointReachRadius))
	{
		bool bSet = false;
		
		if (bUseNavigation)
		{
			if (UNavigationSystemV1* NS = UNavigationSystemV1::GetCurrent(GetWorld()))
			{
				FNavLocation NL;

				//HomeLocation 기준으로 Radius 반경 내에서 갈 수 있는 랜덤 지점 찾기
				if (NS->GetRandomPointInNavigableRadius(HomeLocation, PatrolRoamRadius, NL))
				{
					PatrolGoal = NL.Location; // 찾은 위치로 골 위치 지정
					bSet = true; //목적지 찾기 성공
				}
			}
		}
		if (!bSet) // NavMesh 없다면 → 랜덤 2D 산책
		{
			const float R = FMath::FRandRange(PatrolRoamRadius * 0.4f, PatrolRoamRadius);
			const float A = FMath::FRandRange(0.f, 2 * PI);
			\
			// 최종 목표 지점을 계산. (HomeLocation + 방향 * 거리)
			PatrolGoal = HomeLocation + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 0.f);
		}
		RequestMoveTo(PatrolGoal, PatrolPointReachRadius);
	}
}

void ACEnemyCharacterBase::DoAlert()
{
	const float AlertDuration = 0.4f;
	const bool bLoS = Target && HasVisualOnTarget();
	const bool bNear = Target && DistToTarget() <= ChaseStartDistance;

	if (!bLoS || !bNear)
	{
		SetState(EEnemyState::Patrol);
		return;
	}

	if (GetWorld()->GetTimeSeconds() - StateEnterTime >= AlertDuration)
	{
		SetState(EEnemyState::Chase);
	}
}

void ACEnemyCharacterBase::DoChase()
{
	if (!Target)
	{
		SetState(EEnemyState::ReturnHome);
		return;
	}

	const float Dist = DistToTarget();

	// ── 포위(링) 오프셋 목표 ──
	FVector Goal = Target->GetActorLocation();
	if (bUseChaseRing)
	{
		const float r = FMath::Max(AttackEnterDistance + ChaseRingPadding, AttackEnterDistance * 1.15f);
		const float PlayerYaw = Target->GetActorRotation().Yaw;
		const float AngleDeg = PlayerYaw + MyChaseAngleDeg;
		const FVector Dir = FRotationMatrix(FRotator(0.f, AngleDeg, 0.f)).GetUnitAxis(EAxis::X);
		Goal = Target->GetActorLocation() - Dir * r;

		if (bShowDebugInfo)
			DrawDebugSphere(GetWorld(), Goal, 16.f, 8, FColor::Purple, false, 0.1f);
	}

	// 추적 → 공격
	if (Dist <= AttackEnterDistance && HasVisualOnTarget())
	{
		StopMove();
		SetState(EEnemyState::Attack);
		return;
	}

	if (!HasVisualOnTarget())
	{
		const bool bLoseTooLong = (GetWorld()->GetTimeSeconds() - LastSeenTime) >= LoseSightGrace;
		if (bLoseTooLong || Dist >= ChaseStopDistance)
		{
			SetState(EEnemyState::ReturnHome);
			return;
		}
	}
	else
	{
		LastSeenTime = GetWorld()->GetTimeSeconds();
	}

	RequestMoveTo(Goal, PatrolPointReachRadius);
}

void ACEnemyCharacterBase::DoAttack()
{
	if (!Target || DistToTarget() >= AttackExitDistance)
	{
		SetState(EEnemyState::Chase);
		return;
	}

	// 기본: 주기마다 단발 판정(스윙 창 쓰면 Tick에서 반복 스윕)
	if (IsAttackReady())
	{
		const bool bHit = ApplyAttackDamage(/*bCheckAngle=*/true);
		UE_LOG(LogTemp, Verbose, TEXT("[Enemy] DoAttack: try -> hit=%d"), bHit ? 1 : 0);

		// 실제 공격 시점으로 주기 갱신
		LastAttackTime = GetWorld()->GetTimeSeconds();
	}
}

void ACEnemyCharacterBase::DoReturnHome()
{
	if (Reached(HomeLocation, PatrolPointReachRadius))
	{
		SetState(EEnemyState::Patrol);
		return;
	}
	RequestMoveTo(HomeLocation, PatrolPointReachRadius);
}

void ACEnemyCharacterBase::DoDead()
{
	StopMove();
	// 사망 연출은 OnDead에서
}



// ─────────────────────────────────────────────────────────────────────────────
// 조건/헬퍼
// ─────────────────────────────────────────────────────────────────────────────
bool ACEnemyCharacterBase::IsAttackReady() const
{
	return (GetWorld()->GetTimeSeconds() - LastAttackTime) >= AttackInterval;
}

bool ACEnemyCharacterBase::IsInAttackDistance() const
{
	return Target && (DistToTarget() <= AttackEnterDistance);
}

bool ACEnemyCharacterBase::AcquireTarget()
{
	// 기존 타겟 유효성
	// if (Target && !Target->IsPendingKill() && Target->IsA<ACPlayerCharacter>())
	// 	  return true;

	// IsPendingKill() -> C4996 Warning 발생으로 코드 변경
	if (IsValid(Target) && Target->IsA<ACPlayerCharacter>())
		return true;
	
	// 가장 가까운 플레이어 탐색
	TArray<AActor*> Players;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACPlayerCharacter::StaticClass(), Players);

	if (Players.Num() == 0)
	{
		Target = nullptr;
		return false;
	}

	AActor* Closest = nullptr;
	float MinDist = FLT_MAX;

	for (AActor* P : Players)
	{
		const float D = FVector::Dist(GetActorLocation(), P->GetActorLocation());
		if (D < MinDist) { MinDist = D; Closest = P; }
	}

	Target = Closest;
	return (Target != nullptr);
}

bool ACEnemyCharacterBase::HasVisualOnTarget() const
{
	if (!Target) return false;

	const float DistSq = FVector::DistSquared(GetActorLocation(), Target->GetActorLocation());
	if (DistSq > FMath::Square(SightDistance)) return false;

	if (!IsTargetInFOV(Target)) return false;

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PF_AI_LOS), false, this);
	const FVector S = GetActorLocation() + FVector(0.f, 0.f, SightHeightOffsetSelf);
	const FVector E = Target->GetActorLocation() + FVector(0.f, 0.f, SightHeightOffsetTarget);
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, S, E, SightTraceChannel, Params);

	return !bHit || Hit.GetActor() == Target;
}

bool ACEnemyCharacterBase::IsTargetInFOV(const AActor* Other) const
{
	if (!Other) return false;

	FVector Fwd = GetActorForwardVector(); Fwd.Z = 0.f; Fwd.Normalize();
	FVector To = Other->GetActorLocation() - GetActorLocation(); To.Z = 0.f;
	if (!To.Normalize()) return false;

	const float CosHalf = FMath::Cos(FMath::DegreesToRadians(SightFOVDegrees * 0.5f));
	const float Dot = FVector::DotProduct(Fwd, To);
	return (Dot >= CosHalf);
}

float ACEnemyCharacterBase::DistToTarget() const
{
	return Target ? FVector::Dist(GetActorLocation(), Target->GetActorLocation()) : FLT_MAX;
}

// ─ 이동(Nav/직진) ─
void ACEnemyCharacterBase::RequestMoveTo(const FVector& Goal, float AcceptanceRadius)
{
	if (bUseNavigation)
	{
		if (AAIController* AI = Cast<AAIController>(GetController()))
		{
			FAIMoveRequest Req(Goal);
			Req.SetAcceptanceRadius(AcceptanceRadius);
			AI->MoveTo(Req);
		}
	}
	else
	{
		DirectMoveGoal = Goal;
		DirectAcceptanceRadius = AcceptanceRadius;
		bDirectMoveActive = true;
	}
}

void ACEnemyCharacterBase::StopMove()
{
	if (bUseNavigation)
	{
		if (AAIController* AI = Cast<AAIController>(GetController()))
			AI->StopMovement();
	}
	else
	{
		bDirectMoveActive = false;
		if (UCharacterMovementComponent* M = GetCharacterMovement())
			M->StopMovementImmediately();
	}
}

bool ACEnemyCharacterBase::Reached(const FVector& P, float Radius) const
{
	return FVector::DistSquared(GetActorLocation(), P) <= FMath::Square(Radius);
}

void ACEnemyCharacterBase::DirectMoveTick(float /*DeltaSeconds*/)
{
	if (!bDirectMoveActive) return;

	if (Reached(DirectMoveGoal, DirectAcceptanceRadius))
	{
		bDirectMoveActive = false;
		return;
	}
	
	FVector Dir = (DirectMoveGoal - GetActorLocation());
	Dir.Z = 0.f;
	
	if (Dir.IsNearlyZero())
		return;
	
	Dir.Normalize();
	
	// Separation: 주변 적 밀어내기
	if (bUseSeparation)
	{
		TArray<FOverlapResult> Overlaps;
		FCollisionObjectQueryParams Obj; Obj.AddObjectTypesToQuery(ECC_Pawn);
		FCollisionQueryParams Q(SCENE_QUERY_STAT(PF_AI_Separation), false, this);

		const bool bAny = GetWorld()->OverlapMultiByObjectType(
			Overlaps, GetActorLocation(), FQuat::Identity,
			Obj, FCollisionShape::MakeSphere(SeparationRadius), Q
		);

		if (bAny)
		{
			FVector Sep = FVector::ZeroVector;
			int32 Count = 0;

			for (const FOverlapResult& O : Overlaps)
			{
				const ACEnemyCharacterBase* Other = Cast<ACEnemyCharacterBase>(O.GetActor());
				if (!Other || Other == this) continue;

				FVector Away = GetActorLocation() - Other->GetActorLocation();
				Away.Z = 0.f;
				const float D = Away.Size2D();
				if (D < KINDA_SMALL_NUMBER) continue;

				Away /= D;
				Sep += Away * (1.f - FMath::Clamp(D / SeparationRadius, 0.f, 1.f));
				++Count;
			}

			if (Count > 0)
			{
				Sep /= (float)Count;
				Dir = (Dir * 1.f + Sep * (SeparationStrength / 300.f));
				Dir.Z = 0.f;
				Dir.Normalize();
			}
		}
	}

	AddMovementInput(Dir, 1.f);

}


// ─────────────────────────────────────────────────────────────────────────────
// 데미지/사망/드롭
// ─────────────────────────────────────────────────────────────────────────────
/*float ACEnemyCharacterBase::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}
*/

float ACEnemyCharacterBase::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	UE_LOG(LogTemp, Warning, TEXT("[%s] TakeDamage 호출됨! 데미지: %.1f, 공격자: %s"), 
		   *GetName(), DamageAmount, *GetNameSafe(DamageCauser));
    
	float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    
	// HealthComponent 찾아서 데미지 적용
	if (HealthComponent)
	{
		float OldHealth = HealthComponent->GetHealth();
		HealthComponent->Damage(ActualDamage);
		float NewHealth = HealthComponent->GetHealth();
        
		UE_LOG(LogTemp, Warning, TEXT("[%s] 체력 변화: %.1f -> %.1f"), 
			   *GetName(), OldHealth, NewHealth);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] HealthComponent를 찾을 수 없음!"), *GetName());
	}
    
	return ActualDamage;
}

void ACEnemyCharacterBase::OnHealthChanged(float Cur, float Max)
{
	if (Cur <= 0.f && State != EEnemyState::Dead)
	{
		SetState(EEnemyState::Dead);
		OnDead();
	}
}

void ACEnemyCharacterBase::OnDead()
{
	if (UCharacterMovementComponent* M = GetCharacterMovement())
		M->DisableMovement();
	if (UCapsuleComponent* Cap = GetCapsuleComponent())
		Cap->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	bAttackWindowActive = false;
	SwingHitActors.Reset();
	AttackWindowEndTime = -1.f;

	TryDropHealPack();
	SetLifeSpan(3.0f);
}

void ACEnemyCharacterBase::TryDropHealPack()
{
	if (!HealPackClass) return;
	if (FMath::FRand() > HealPackDropChance) return;

	/*
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	GetWorld()->SpawnActor<AActor>(HealPackClass, GetActorLocation(), FRotator::ZeroRotator, P);
	*/
	
	// 힐 오브 드랍(풀 우선) - 
	const FVector  SpawnLoc = GetActorLocation();
	const FRotator SpawnRot = FRotator::ZeroRotator;
	
	if (UWorld* World = GetWorld())
	{
		if (UCHealOrbPoolSubsystem* Pool = GetGameInstance()->GetSubsystem<UCHealOrbPoolSubsystem>())
		{
			// 풀 초기 클래스가 지정되어 있어야 합니다(게임 시작 시 1회 SetOrbClass)
			const FTransform Xform(SpawnRot, SpawnLoc);
			AActor* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0);
			Pool->Acquire(World, Xform, PlayerPawn);
		}
	}
	
}


// ─────────────────────────────────────────────────────────────────────────────
// 전투: 스윙 창 + 단발 스윕 + 분할 스윕
// ─────────────────────────────────────────────────────────────────────────────

void ACEnemyCharacterBase::AttackWindowBegin(float AutoEndAfter /*=0.f*/)
{
	bAttackWindowActive = true;
	SwingHitActors.Reset();

	const float Now = GetWorld()->GetTimeSeconds();
	LastAttackSweepTime = Now;
	AttackWindowEndTime = (AutoEndAfter > 0.f) ? (Now + AutoEndAfter) : -1.f;
}

void ACEnemyCharacterBase::AttackWindowEnd(bool bForce /*=true*/)
{
	if (bForce)
	{
		bAttackWindowActive = false;
		AttackWindowEndTime = -1.f;
		SwingHitActors.Reset();
		return;
	}

	if (AttackWindowEndTime < 0.f)
		AttackWindowEndTime = GetWorld()->GetTimeSeconds();
}

// 단발 판정 (스윕 + Overlap 보조 + 거리 안전망)
bool ACEnemyCharacterBase::ApplyAttackDamage(bool bCheckAngle /*=true*/)
{
    UWorld* W = GetWorld();
    if (!W) return false;

    // 멀티 전용 처리 원하시면:
    // if (!HasAuthority()) return false;

    const FVector Origin = GetActorLocation() + AttackOriginOffset;
    const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
    const FVector End = Origin + Forward * AttackSweepLength;

    const FQuat Rot = FRotationMatrix::MakeFromX(Forward).ToQuat();
    const FCollisionShape Shape = FCollisionShape::MakeCapsule(AttackSweepRadius, AttackSweepHalfHeight);

    TArray<FHitResult> Hits;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(PF_AttackSweep_Immediate), false, this);

    bool bHitSomething = false;
    const bool bAnySweep = W->SweepMultiByChannel(Hits, Origin, End, Rot, AttackTraceChannel, Shape, Params);

    if (bDebugDrawAttack)
    {
        DrawDebugCapsule(W, Origin, AttackSweepHalfHeight, AttackSweepRadius, Rot, FColor::Yellow, false, 0.1f, 0, 1.5f);
        DrawDebugCapsule(W, End, AttackSweepHalfHeight, AttackSweepRadius, Rot, FColor::Orange, false, 0.1f, 0, 1.5f);
        DrawDebugLine(W, Origin, End, FColor::Cyan, false, 0.1f, 0, 1.5f);
    }

    if (bAnySweep)
    {
        for (const FHitResult& H : Hits)
        {
            AActor* A = H.GetActor();
            if (!A || A == this) continue;
            if (SwingHitActors.Contains(A)) continue;
            if (bCheckAngle && !PassAngleFilter(A)) continue;
            if (bAttackHitOnlyPlayers && !Cast<APawn>(A)) continue;

            SwingHitActors.Add(A);
            UGameplayStatics::ApplyDamage(A, BaseDamage, GetController(), this, UDamageType::StaticClass());
            bHitSomething = true;

            if (bDebugDrawAttack)
                DrawDebugPoint(W, H.ImpactPoint, 10.f, FColor::Red, false, 0.2f);
        }
    }
    else
    {
        // ── 보조 1: Pawn ObjectType Overlap (채널 미스매치 대비)
        {
            FCollisionObjectQueryParams ObjParams; ObjParams.AddObjectTypesToQuery(ECC_Pawn);
            FCollisionQueryParams QParams(SCENE_QUERY_STAT(PF_AttackOverlap), false, this);

            TArray<FOverlapResult> Overlaps;
            const bool bOver = W->OverlapMultiByObjectType(
                Overlaps, End, Rot,
                ObjParams, FCollisionShape::MakeCapsule(AttackSweepRadius, AttackSweepHalfHeight),
                QParams
            );

            if (bOver)
            {
                for (const FOverlapResult& O : Overlaps)
                {
                    AActor* A = O.GetActor();
                    if (!A || A == this) continue;
                    if (SwingHitActors.Contains(A)) continue;
                    if (bCheckAngle && !PassAngleFilter(A)) continue;
                    if (bAttackHitOnlyPlayers && !Cast<APawn>(A)) continue;

                    SwingHitActors.Add(A);
                    UGameplayStatics::ApplyDamage(A, BaseDamage, GetController(), this, UDamageType::StaticClass());
                    bHitSomething = true;

                    if (bDebugDrawAttack)
                        DrawDebugPoint(W, A->GetActorLocation(), 10.f, FColor::Magenta, false, 0.2f);
                }
            }
        }

        // ── 보조 2: AttackRange 거리 안전망
        if (!bHitSomething && Target && AttackRange > 0.f
            && FVector::Dist(GetActorLocation(), Target->GetActorLocation()) <= AttackRange)
        {
            if (!SwingHitActors.Contains(Target) && (!bCheckAngle || PassAngleFilter(Target)))
            {
                SwingHitActors.Add(Target);
                UGameplayStatics::ApplyDamage(Target, BaseDamage, GetController(), this, UDamageType::StaticClass());
                bHitSomething = true;
            }
        }
    }

    UE_LOG(LogTemp, Verbose, TEXT("[Enemy] ApplyAttackDamage: hit=%d"), bHitSomething ? 1 : 0);
    return bHitSomething;
}

// 분할 스윕(프레임 독립)
void ACEnemyCharacterBase::PerformAttackSweep()
{
    UWorld* W = GetWorld();
    if (!W) return;

    // if (!HasAuthority()) return;

    const float Now = W->GetTimeSeconds();

    // 자동 종료
    if (AttackWindowEndTime > 0.f && Now >= AttackWindowEndTime)
    {
        AttackWindowEnd(/*bForce=*/true);
        return;
    }

    // 경과 시간 기반 필요한 스윕 횟수
    float Elapsed = Now - LastAttackSweepTime;
    if (Elapsed < KINDA_SMALL_NUMBER)
        Elapsed = AttackSweepInterval; // 최소 1회 보장

    int32 Needed = FMath::Max(1, FMath::FloorToInt(Elapsed / AttackSweepInterval));
    Needed = FMath::Min(Needed, MaxSweepsPerFrame);

    const FVector OriginBase = GetActorLocation() + AttackOriginOffset;
    const FVector ForwardBase = GetActorForwardVector().GetSafeNormal2D();
    const FQuat   RotBase = FRotationMatrix::MakeFromX(ForwardBase).ToQuat();
    const FCollisionShape Shape = FCollisionShape::MakeCapsule(AttackSweepRadius, AttackSweepHalfHeight);

    for (int32 i = 0; i < Needed; ++i)
    {
        const float T0 = (float)i / (float)Needed;
        const float T1 = (float)(i + 1) / (float)Needed;

        const FVector Start = OriginBase + ForwardBase * (AttackSweepLength * T0);
        const FVector End = OriginBase + ForwardBase * (AttackSweepLength * T1);

        TArray<FHitResult> Hits;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(PF_AttackSweep_Seg), false, this);

        const bool bAny = W->SweepMultiByChannel(Hits, Start, End, RotBase, AttackTraceChannel, Shape, Params);

        if (bDebugDrawAttack)
        {
            DrawDebugCapsule(W, Start, AttackSweepHalfHeight, AttackSweepRadius, RotBase, FColor::Yellow, false, 0.f, 0, 1.2f);
            DrawDebugCapsule(W, End, AttackSweepHalfHeight, AttackSweepRadius, RotBase, FColor::Orange, false, 0.f, 0, 1.2f);
            DrawDebugLine(W, Start, End, FColor::Cyan, false, 0.f, 0, 1.2f);
        }

        if (!bAny) continue;

        for (const FHitResult& H : Hits)
        {
            AActor* A = H.GetActor();
            if (!A || A == this)           continue;
            if (SwingHitActors.Contains(A)) continue;
            if (!PassAngleFilter(A))        continue;
            if (bAttackHitOnlyPlayers && !Cast<APawn>(A)) continue;

            SwingHitActors.Add(A);
            UGameplayStatics::ApplyDamage(A, BaseDamage, GetController(), this, UDamageType::StaticClass());

            if (bDebugDrawAttack)
                DrawDebugPoint(W, H.ImpactPoint, 10.f, FColor::Red, false, 0.2f);
        }
    }

    LastAttackSweepTime = Now;
}

// 전방 콘 필터
bool ACEnemyCharacterBase::PassAngleFilter(const AActor* Other) const
{
	if (!Other) return false;

	FVector Fwd = GetActorForwardVector(); Fwd.Z = 0.f; Fwd.Normalize();
	FVector To = Other->GetActorLocation() - GetActorLocation(); To.Z = 0.f;
	if (!To.Normalize()) return false;

	const float CosHalf = FMath::Cos(FMath::DegreesToRadians(AttackArcDegrees * 0.5f));
	const float Dot = FVector::DotProduct(Fwd, To);
	return (Dot >= CosHalf);
}

void ACEnemyCharacterBase::DebugDrawState()
{
}



