#include "CEnemyCharacterBase.h"

#include "AIController.h"
#include "BrainComponent.h"
#include "NavigationSystem.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "Navigation/PathFollowingComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"

#include "Global.h"
#include "NiagaraFunctionLibrary.h"
#include "00_Character/00_Player/CPlayerCharacter.h"
#include "00_Character/02_Component/00_PlayerComponent/CFuryGaugeComponent.h"
#include "00_Character/02_Component/00_PlayerComponent/CPlayerWeaponComponent.h" 
#include "00_Character/02_Component/01_EnemyComponent/CEnemyHealthComponent.h"
#include "01_AIController/CTacticalEnemyAIController.h"
#include "01_Item/CHealOrbPoolSubsystem.h"
#include "03_Combat/Damage/DamageType_FuryCountable.h"

#include "05_System/01_Sound/CSoundManagerSubsystem.h"
#include "05_System/01_Sound//CSoundDataAsset.h"
#include "05_System/CPawnLifecycleSubsystem.h"
#include "00_Character/CMainGameModeBase.h"
#include "Engine/GameInstance.h"

#include "Engine/DamageEvents.h"

ACEnemyCharacterBase::ACEnemyCharacterBase()
{
	PrimaryActorTick.bCanEverTick = true;

	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = ACTacticalEnemyAIController::StaticClass();
	
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
		M->MaxStepHeight = FMath::Max(60.f, M->MaxStepHeight);
		M->bCanWalkOffLedges = true;
			
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

	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		OriginalMeshLocation = MeshComp->GetRelativeLocation();
	}


	// 플레이어 콤보 히트 델리게이트 구독함수
	SubscribeToPlayerComboHits();
	
	SetState(EEnemyState::Patrol); // 기본적으로 순찰모드
	CacheSoundsFromDataAsset();
	RegisterForPlayerRespawnEvents();

}



void ACEnemyCharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnsubscribeFromPlayerComboHits();
	StopHitShake();
	UnregisterFromPlayerRespawnEvents();
	Super::EndPlay(EndPlayReason);
}

void ACEnemyCharacterBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (State == EEnemyState::Dead || bIsHitStunned)
	{
		return;
	}
	
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

void ACEnemyCharacterBase::RegisterForPlayerRespawnEvents()
{
	if (PlayerRespawnDelegateHandle.IsValid())
	{
		return;
	}
	
	UWorld* World = GetWorld();
	if (!World)
		return;
	
	if (UGameInstance* GameInstance = World->GetGameInstance())
	{
		if (UCPawnLifecycleSubsystem* Lifecycle = GameInstance->GetSubsystem<UCPawnLifecycleSubsystem>())
		{
			PlayerRespawnDelegateHandle = Lifecycle->OnPlayerRespawned().AddUObject(this, &ACEnemyCharacterBase::HandlePlayerRespawned);
		}
	}
}

void ACEnemyCharacterBase::UnregisterFromPlayerRespawnEvents()
{
	if (!PlayerRespawnDelegateHandle.IsValid())
	{
		return;
	}
	
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (UCPawnLifecycleSubsystem* Lifecycle = GameInstance->GetSubsystem<UCPawnLifecycleSubsystem>())
			{
				Lifecycle->OnPlayerRespawned().Remove(PlayerRespawnDelegateHandle);
			}
		}
	}
	
	PlayerRespawnDelegateHandle = FDelegateHandle();
}

void ACEnemyCharacterBase::HandlePlayerRespawned(ACPlayerCharacter* NewPlayer)
{
	if (!IsValid(NewPlayer) || State == EEnemyState::Dead)
	{
		return;
	}

	ResetForRespawn();
	ForceRestartAI();
	OnRespawned();

	Target = NewPlayer;
	NextThinkTime = 0.f;

	if (GetWorld())
	{
		LastSeenTime = GetWorld()->GetTimeSeconds();
	}
	
	if (State == EEnemyState::Patrol || State == EEnemyState::Alert || State == EEnemyState::ReturnHome)
	{
		SetState(EEnemyState::Chase);
	}
}



// ─────────────────────────────────────────────────────────────────────────────
// FSM
// ─────────────────────────────────────────────────────────────────────────────
void ACEnemyCharacterBase::Think(float /*DeltaTime*/)
{
	if (bIsHitStunned)
		return;
	
	if (State == EEnemyState::Dead)
		return;
	
	
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


void ACEnemyCharacterBase::EnterState(EEnemyState NewState)
{
	if (const UWorld* World = GetWorld())
	{
		const float Now = World->GetTimeSeconds();
		StateEnterTime = Now;
		
		if (NewState == EEnemyState::Chase)
		{
			LastSeenTime = Now;
		}
	}
	else
	{
		// PIE 편집 등 월드가 없는 컨텍스트에서 호출될 수 있으므로
		StateEnterTime = 0.f;
		
		if (NewState == EEnemyState::Chase)
		{
			LastSeenTime = 0.f;
		}
	}
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
	if (Target)
	{
		const float Dist = DistToTarget();
			
		// Patrol → Alert : 거리 기반 인지
		if (Dist <= ChaseStartDistance)
		{
			SetState(EEnemyState::Alert);
			return;
		}
 

		// 너무 멀어지면 타겟 해제
		if (Dist >= ChaseStopDistance)
		{
			Target = nullptr;
		}
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
            
			// 최종 목표 지점을 계산. (HomeLocation + 방향 * 거리)
			PatrolGoal = HomeLocation + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 0.f);
		}
	}
	
	if (!PatrolGoal.IsNearlyZero())
	{
		RequestMoveTo(PatrolGoal, PatrolPointReachRadius);
	}
}

void ACEnemyCharacterBase::DoAlert()
{
	const float AlertDuration = 0.4f;
	const bool bNear = Target && DistToTarget() <= ChaseStartDistance;

	if (!bNear)
	{
		SetState(EEnemyState::Patrol);
		return;
	}

	if (GetWorld()->GetTimeSeconds() - StateEnterTime >= AlertDuration)
	{
		LastSeenTime = GetWorld()->GetTimeSeconds();
		SetState(EEnemyState::Chase);
	}
}

void ACEnemyCharacterBase::DoChase()
{

	if (bIsHitStunned)
		return;
	
	if (!Target)
	{
		SetState(EEnemyState::ReturnHome);
		return;
	}

	const float Dist = DistToTarget();

	if (Dist >= ChaseStopDistance || (!HasVisualOnTarget() && GetWorld()->GetTimeSeconds() - LastSeenTime >= LoseSightGrace))
	{
		SetState(EEnemyState::ReturnHome);
		return;
	}
	
	if (Dist <= AttackEnterDistance && HasVisualOnTarget())
	{
		StopMove();
		SetState(EEnemyState::Attack);
		return;
	}
	
	if (HasVisualOnTarget())
	{
		LastSeenTime = GetWorld()->GetTimeSeconds();
	}
	
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

	RequestMoveTo(Goal, PatrolPointReachRadius);
}

void ACEnemyCharacterBase::DoAttack()
{
	if (!Target)
	{
		SetState(EEnemyState::ReturnHome);
		return;
	}

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		if (Move->IsFalling())
		{
			bIsPerformingMelee = false;
			return;
		}
	}

	const float Dist = DistToTarget();

	// Attack → Chase
	if (Dist > AttackExitDistance)
	{
		SetState(EEnemyState::Chase);
		return;
	}

	if (!bIsPerformingMelee)
	{
		// 타겟에 접근
		const float Approach = AttackMoveAcceptanceRadius;
		if (Dist > Approach)
		{
			if (bUseNavigation)
			{
				RequestMoveTo(Target->GetActorLocation(), Approach);
			}
			else
			{
				FVector dir = Target->GetActorLocation() - GetActorLocation();
				dir.Z = 0.f;
				if (dir.Normalize())
					AddMovementInput(dir, 1.f);
			}
		}
		else
		{
			StopMove();
		}

		// 공격 수행 조건
		if (Dist <= AttackEnterDistance && IsAttackReady() && HasVisualOnTarget())
		{
			bIsPerformingMelee = true;
			LastAttackTime = GetWorld()->GetTimeSeconds();

			// Base 클래스의 스윙창 열기
			AttackWindowBegin(0.5f);
		}
	}
	else
	{
		StopMove();
	}
}

void ACEnemyCharacterBase::DoReturnHome()
{
	const float Dist2D = FVector::Dist2D(GetActorLocation(), HomeLocation);

	if (Dist2D <= PatrolPointReachRadius)
	{
		Target = nullptr;
		SetState(EEnemyState::Patrol);
		return;
	}

	RequestMoveTo(HomeLocation, PatrolPointReachRadius);

	if (Target && DistToTarget() <= ChaseStartDistance)
	{
		LastSeenTime = GetWorld()->GetTimeSeconds();
		SetState(EEnemyState::Chase);
	}
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
	//return (GetWorld()->GetTimeSeconds() - LastAttackTime) >= AttackInterval;
	// AttackInterval은 CEnemyCharacterBase.h 또는 CRiotRobot.h에 UPROPERTY로 정의되어 있을 것입니다.
	if (GetWorld() && AttackInterval > 0.f)
	{
		return GetWorld()->GetTimeSeconds() >= LastAttackTime + AttackInterval;
	}
	return true; // AttackInterval이 0이하면 항상 공격 가능
}

bool ACEnemyCharacterBase::IsInAttackDistance() const
{
	return Target && DistToTarget() <= AttackEnterDistance;
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



// ─────────────────────────────────────────────────────────────────────────────
// 이동
// ─────────────────────────────────────────────────────────────────────────────
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
		FCollisionObjectQueryParams Obj;
		Obj.AddObjectTypesToQuery(ECC_Pawn);
		Obj.AddObjectTypesToQuery(PF::Collision::RiotEnemy);
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

void ACEnemyCharacterBase::UpdateHitDirectionFromAttacker(AActor* AttackerActor)
{
	LastHitDirection = EEnemyHitDirection::None;
	LastHitDirectionRightDot = 0.f;

	if (!IsValid(AttackerActor))
	{
		return;
	}

	if (State == EEnemyState::Dead)
	{
		return;
	}

	if (const UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		if (!Movement->IsMovingOnGround())
		{
			if (bShowDebugInfo)
			{
				UE_LOG(LogTemp, Verbose, TEXT("[%s] Skipping directional hit react - not on ground"), *GetName());
			}
			return;
		}
	}

	FVector DirToAttacker = AttackerActor->GetActorLocation() - GetActorLocation();
	DirToAttacker.Z = 0.f;
	if (!DirToAttacker.Normalize())
	{
		return;
	}

	FVector EnemyRight = GetActorRightVector();
	EnemyRight.Z = 0.f;
	if (!EnemyRight.Normalize())
	{
		FVector EnemyForward = GetActorForwardVector();
		EnemyForward.Z = 0.f;
		if (EnemyForward.Normalize())
		{
			EnemyRight = FVector::CrossProduct(FVector::UpVector, EnemyForward).GetSafeNormal();
		}
	}

	if (!EnemyRight.Normalize())
	{
		return;
	}

	LastHitDirectionRightDot = FVector::DotProduct(DirToAttacker, EnemyRight);

	constexpr float DirectionDeadzone = 0.1f;
	if (LastHitDirectionRightDot > DirectionDeadzone)
	{
		LastHitDirection = EEnemyHitDirection::FromRight;
	}
	else if (LastHitDirectionRightDot < -DirectionDeadzone)
	{
		LastHitDirection = EEnemyHitDirection::FromLeft;
	}

	if (bShowDebugInfo)
	{
		const TCHAR* DirText = TEXT("None");
		switch (LastHitDirection)
		{
		case EEnemyHitDirection::FromLeft: DirText = TEXT("FromLeft"); break;
		case EEnemyHitDirection::FromRight: DirText = TEXT("FromRight"); break;
		default: break;
		}

		UE_LOG(LogTemp, Log, TEXT("[%s] Hit direction resolved: DotRight=%.3f -> %s (Attacker: %s)"),
				*GetName(), LastHitDirectionRightDot, DirText, *GetNameSafe(AttackerActor));
	}
}

UAnimMontage* ACEnemyCharacterBase::ResolveComboHitReactionMontage(int32 ComboIndex, FName& OutSource) const
{
	OutSource = NAME_None;

	auto TryResolve = [ComboIndex](const TArray<UAnimMontage*>* Montages) -> UAnimMontage*
	{
		if (!Montages)
			return nullptr;

		if (Montages->IsValidIndex(ComboIndex))
		{
			return (*Montages)[ComboIndex];
		}

		return nullptr;
	};

	const TArray<UAnimMontage*>* DirectionalComboArray = nullptr;
	const TArray<UAnimMontage*>* DirectionalFallbackArray = nullptr;

	switch (LastHitDirection)
	{
	case EEnemyHitDirection::FromLeft:
		DirectionalComboArray = &ComboHitReactionMontagesLeft;
		DirectionalFallbackArray = &HitReactionMontagesLeft;
		break;
	case EEnemyHitDirection::FromRight:
		DirectionalComboArray = &ComboHitReactionMontagesRight;
		DirectionalFallbackArray = &HitReactionMontagesRight;
		break;
	default:
		break;
	}
	
	if (UAnimMontage* Montage = TryResolve(DirectionalComboArray))
	{
		OutSource = (LastHitDirection == EEnemyHitDirection::FromLeft)
				? FName(TEXT("DirectionalCombo_Left"))
				: FName(TEXT("DirectionalCombo_Right"));
		return Montage;
	}

	if (ComboHitReactionMontages.IsValidIndex(ComboIndex) && ComboHitReactionMontages[ComboIndex])
	{
		OutSource = FName(TEXT("DefaultCombo"));
		return ComboHitReactionMontages[ComboIndex];
	}

	if (UAnimMontage* Montage = TryResolve(DirectionalFallbackArray))
	{
		OutSource = (LastHitDirection == EEnemyHitDirection::FromLeft)
				? FName(TEXT("DirectionalFallback_Left"))
				: FName(TEXT("DirectionalFallback_Right"));
		return Montage;
	}

	if (HitReactionMontages.IsValidIndex(ComboIndex) && HitReactionMontages[ComboIndex])
	{
		OutSource = FName(TEXT("DefaultFallback"));
		return HitReactionMontages[ComboIndex];
	}

	return nullptr;
}

float ACEnemyCharacterBase::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	UE_LOG(LogTemp, Warning, TEXT("[%s] TakeDamage 호출됨! 데미지: %.1f, 공격자: %s"), 
		   *GetName(), DamageAmount, *GetNameSafe(DamageCauser));
	
	
	float Applied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (Applied <= 0.0f) return Applied;

	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent* PointDamage = static_cast<const FPointDamageEvent*>(&DamageEvent);
		SpawnDamageReceivedEffect(PointDamage->HitInfo.ImpactPoint, PointDamage->HitInfo.ImpactNormal);
	}
	else
	{
		FVector HitLocation = GetActorLocation();
		FVector HitNormal = DamageCauser ? (GetActorLocation() - DamageCauser->GetActorLocation()).GetSafeNormal() : FVector::UpVector;
		SpawnDamageReceivedEffect(HitLocation, HitNormal);
	}
	
	APawn* InstigatorPawn = EventInstigator ? EventInstigator->GetPawn() : nullptr;
	if (!InstigatorPawn && DamageCauser)
	{
		InstigatorPawn = Cast<APawn>(DamageCauser);
		if (!InstigatorPawn)
		{
			InstigatorPawn = DamageCauser->GetInstigator();
		}
			
		if (!InstigatorPawn && DamageCauser->GetOwner())
		{
			InstigatorPawn = Cast<APawn>(DamageCauser->GetOwner());
		}
	}

	if (ACPlayerCharacter* PlayerInstigator = Cast<ACPlayerCharacter>(InstigatorPawn))
	{
		UpdateHitDirectionFromAttacker(PlayerInstigator);
			
		Target = PlayerInstigator;
		LastSeenTime = GetWorld()->GetTimeSeconds();
			
		switch (State)
		{
		case EEnemyState::Patrol:
		case EEnemyState::Alert:
		case EEnemyState::ReturnHome:
			SetState(EEnemyState::Chase);
			break;
		default:
			break;
		}
	}
	else
	{
		LastHitDirection = EEnemyHitDirection::None;
		LastHitDirectionRightDot = 0.f;
	}
	
	const bool bCountsForFury = DamageEvent.DamageTypeClass && DamageEvent.DamageTypeClass->IsChildOf(UDamageType_FuryCountable::StaticClass());

	if (bCountsForFury && EventInstigator)
	{
		if (APawn* InstPawn = EventInstigator->GetPawn())
		{
			if (UCFuryGaugeComponent* Fury = InstPawn->FindComponentByClass<UCFuryGaugeComponent>())
				Fury->AddStack(1);
		}
	}

	if (HealthComponent)
	{
		float OldHealth = HealthComponent->GetHealth();
		HealthComponent->Damage(Applied);
		float NewHealth = HealthComponent->GetHealth();
        
		UE_LOG(LogTemp, Warning, TEXT("[%s] 체력 변화: %.1f -> %.1f"), 
			   *GetName(), OldHealth, NewHealth);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] HealthComponent를 찾을 수 없음!"), *GetName());
	}
	
	return Applied;
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
	bIsHitStunned = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HitStunTimer);
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->DisableMovement();
		Movement->StopMovementImmediately();
	}

	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		if (ACTacticalEnemyAIController* TacAI = Cast<ACTacticalEnemyAIController>(AIC))
		{
			TacAI->TacticalStop();
		}
	}

	bAttackWindowActive = false;
	SwingHitActors.Reset();
	AttackWindowEndTime = -1.f;

	LastHitDirection = EEnemyHitDirection::None;
	LastHitDirectionRightDot = 0.f;

	TryDropHealPack();
}

void ACEnemyCharacterBase::TryDropHealPack()
{
	if (!HealPackClass) return;
	if (FMath::FRand() > HealPackDropChance) return;
	
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

void ACEnemyCharacterBase::SaveInitialTransform()
{
	InitialSpawnLocation = GetActorLocation();
	InitialSpawnRotation = GetActorRotation();
}

void ACEnemyCharacterBase::ResetToInitialTransform()
{
	SetActorLocation(InitialSpawnLocation);
	SetActorRotation(InitialSpawnRotation);

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->Velocity = FVector::ZeroVector;
	}
}

void ACEnemyCharacterBase::ResetForRespawn()
{
	// 상태 초기화
	Target = nullptr;
	const EEnemyState PreviousState = State;
	if (PreviousState != EEnemyState::Patrol)
	{
		SetState(EEnemyState::Patrol);
	}
	else
	{
		ExitState(PreviousState);
		State = EEnemyState::Patrol;
		EnterState(State);
	}
	// 타이머 초기화
	LastSeenTime = -1000.f;
	LastAttackTime = -1000.f;
	StateEnterTime = -1000.f;
	NextThinkTime = 0.f;
    
	// 전투 관련 초기화
	bIsHitStunned = false;
	bIsPerformingMelee = false;
	bAttackWindowActive = false;
	SwingHitActors.Reset();
	AttackWindowEndTime = -1.f;
    
	// 타이머 정리
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HitStunTimer);
	}
    
	// 직진 스티어링 초기화
	bDirectMoveActive = false;

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->SetMovementMode(MOVE_Walking);
		Movement->SetComponentTickEnabled(true);
		Movement->Activate();
		Movement->Velocity = FVector::ZeroVector;
		Movement->SetActive(true);
		Movement->bForceNextFloorCheck = true;
	}

	if (HealthComponent)
	{
		HealthComponent->ResetHealth();
	}

	LastHitDirection = EEnemyHitDirection::None;
	LastHitDirectionRightDot = 0.f;
	
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		if (UAnimInstance* AnimInst = MeshComp->GetAnimInstance())
		{
			AnimInst->Montage_Stop(0.0f);
		}
	}
	
	SetActorEnableCollision(true);
	SetCanBeDamaged(true);

	OnResetForRespawn();
}

void ACEnemyCharacterBase::ForceRestartAI()
{
	AAIController* AICon = Cast<AAIController>(GetController());
	if (!AICon)
	{
		// 컨트롤러가 아예 없다면 스폰
		SpawnDefaultController();
		AICon = Cast<AAIController>(GetController());
		if (!AICon)
		{
			UE_LOG(LogTemp, Error, TEXT("[%s] AI 컨트롤러 스폰 실패!"), *GetName());
			return;
		}
	}

	// 1. 기존 BrainComponent 중지
	if (UBrainComponent* Brain = AICon->GetBrainComponent())
	{
		Brain->StopLogic(TEXT("Player Respawn Reset"));
	}

	// 2. 컨트롤러가 Pawn을 다시 Possess하도록 하여 내부 상태를 리셋
	AICon->UnPossess();
	AICon->Possess(this);

	// 3. CrowdFollowingComponent를 명시적으로 재초기화
	if (ACrowdEnemyAIController* CrowdAI = Cast<ACrowdEnemyAIController>(AICon))
	{
		CrowdAI->ReinitializeCrowdComponent();
	}

	// 4. 새로운 BrainComponent 로직 시작
	if (UBrainComponent* Brain = AICon->GetBrainComponent())
	{
		Brain->StartLogic();
	}
    
	UE_LOG(LogTemp, Log, TEXT("[%s] AI가 성공적으로 재시작 및 재빙의되었습니다."), *GetName());
}

void ACEnemyCharacterBase::OnResetForRespawn_Implementation()
{
}

void ACEnemyCharacterBase::OnRespawned_Implementation()
{
}

void ACEnemyCharacterBase::PlayRandomLaunchReaction()
{
	if (LaunchReactionMontages.Num() == 0)
	{
		CLog::Log(TEXT("LaunchReactionMontages 배열이 비어있습니다."));
		return;
	}

	const int32 RandomIndex = FMath::RandRange(0, LaunchReactionMontages.Num() - 1);
	UAnimMontage* SelectedMontage = LaunchReactionMontages[RandomIndex];

	if (!SelectedMontage)
	{
		CLog::Log(FString::Printf(TEXT("선택된 LaunchReactionMontage[%d]가 nullptr입니다."), RandomIndex));
		return;
	}

	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
		{
			AnimInstance->Montage_Play(SelectedMontage, 1.0f);
			CLog::Log(FString::Printf(TEXT("%s - 띄워지기 애니메이션 재생: 인덱스 %d"), *GetName(), RandomIndex));
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// 전투(스윙 창 + 분할 스윕)
// ─────────────────────────────────────────────────────────────────────────────
void ACEnemyCharacterBase::AttackWindowBegin(float AutoEndAfter)
{
	if (bAttackWindowActive)
		return;

	bAttackWindowActive = true;
	SwingHitActors.Empty();

	LastAttackSweepTime = GetWorld()->GetTimeSeconds();

	if (AutoEndAfter > 0.f)
		AttackWindowEndTime = GetWorld()->GetTimeSeconds() + AutoEndAfter;
	else
		AttackWindowEndTime = -1.f;

	UE_LOG(LogTemp, Verbose, TEXT("[Enemy] AttackWindow opened"));
}

void ACEnemyCharacterBase::AttackWindowEnd(bool bForce)
{
	if (!bAttackWindowActive && !bForce)
		return;

	bAttackWindowActive = false;
	AttackWindowEndTime = -1.f;
	SwingHitActors.Empty();

	bIsPerformingMelee = false;

	UE_LOG(LogTemp, Verbose, TEXT("[Enemy] AttackWindow closed"));
}

bool ACEnemyCharacterBase::IsValidAttackTarget(AActor* Other) const
{
	if (!Other || Other == this)
		return false;

	if (!Other->CanBeDamaged())
		return false;

	if (bAttackHitOnlyPlayers)
	{
		if (!Other->IsA<ACPlayerCharacter>())
			return false;
	}

	return true;
}

bool ACEnemyCharacterBase::ApplyAttackDamage(bool bCheckAngle)
{
    UWorld* W = GetWorld();
    if (!W) return false;

    bool bHitSomething = false;

    if (Target && IsValidAttackTarget(Target) && !SwingHitActors.Contains(Target))
    {
        // ── 보조 1: 캡슐 Overlap 감지(즉시 1회)
        if (USkeletalMeshComponent* SkMesh = GetMesh())
        {
        	TArray<AActor*> OverlappingActors;
        	SkMesh->GetOverlappingActors(OverlappingActors, AActor::StaticClass());

        	for (AActor* A : OverlappingActors)
        	{
        		if (!IsValidAttackTarget(A)) continue;
        		if (SwingHitActors.Contains(A)) continue;
        		if (bCheckAngle && !PassAngleFilter(A)) continue;

        		SwingHitActors.Add(A);
        		UGameplayStatics::ApplyDamage(A, BaseDamage, GetController(), this, UDamageType::StaticClass());
        		bHitSomething = true;

        		if (bDebugDrawAttack)
        			DrawDebugPoint(W, A->GetActorLocation(), 10.f, FColor::Magenta, false, 0.2f);
        	}
        }

        // ── 보조 2: AttackRange 거리 안전망
        if (!bHitSomething && Target && AttackRange > 0.f
            && FVector::Dist(GetActorLocation(), Target->GetActorLocation()) <= AttackRange)
        {
        	if (IsValidAttackTarget(Target) && !SwingHitActors.Contains(Target) && (!bCheckAngle || PassAngleFilter(Target)))
            {
                SwingHitActors.Add(Target);
                UGameplayStatics::ApplyDamage(Target, BaseDamage, GetController(), this, UDamageType::StaticClass());
                bHitSomething = true;
            }
        }
    }

	// 기존 스윙 1회 스윕
	PerformAttackSweep();

	//  공격 접촉 시점을 "최근 시야 확보"로 간주 → ReturnHome 방지
	LastSeenTime = GetWorld()->GetTimeSeconds();

    UE_LOG(LogTemp, Verbose, TEXT("[Enemy] ApplyAttackDamage: hit=%d"), bHitSomething ? 1 : 0);
    return bHitSomething;
}

void ACEnemyCharacterBase::EnsureWalkingAndResume()
{
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		if (Move->MovementMode == MOVE_None)
			Move->SetMovementMode(MOVE_Walking);
	}

	if (bUseNavigation && Target)
	{
		// AI가 StopMovement 이후 바로 다음 프레임엔 MoveTo가 안 들어올 수 있으니 즉시 한 번 걸어줌
		RequestMoveTo(Target->GetActorLocation(), AttackMoveAcceptanceRadius);
	}
}

void ACEnemyCharacterBase::ReengageChase(float DelaySec)
{
	// Attack → Chase 전이(짧은 지연을 주면 애니메이션과 자연스럽게 이어짐)
	if (DelaySec <= KINDA_SMALL_NUMBER)
	{
		SetState(EEnemyState::Chase);
	}
	else
	{
		FTimerHandle TH;
		GetWorld()->GetTimerManager().SetTimer(TH, [this]()
		{
			if (State != EEnemyState::Dead)
				SetState(EEnemyState::Chase);
		}, DelaySec, false);
	}
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
			if (!A)                        continue;
        	if (!IsValidAttackTarget(A))   continue;
            if (SwingHitActors.Contains(A)) continue;
            if (!PassAngleFilter(A))        continue;

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

// ============================================================
// 플레이어 콤보 히트 처리 
// ============================================================

void ACEnemyCharacterBase::SubscribeToPlayerComboHits()
{
	// 플레이어 찾기
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Enemy] SubscribeToPlayerComboHits: PlayerController not found"));
		return;
	}

	APawn* PlayerPawn = PC->GetPawn();
	if (!PlayerPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Enemy] SubscribeToPlayerComboHits: PlayerPawn not found"));
		return;
	}

	// WeaponComponent 직접 찾기
	UCPlayerWeaponComponent* WeaponComp = PlayerPawn->FindComponentByClass<UCPlayerWeaponComponent>();
	if (!WeaponComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Enemy] SubscribeToPlayerComboHits: WeaponComponent not found"));
		return;
	}

	// 델리게이트 바인딩 (간단하고 직접적인 방식)
	if (!WeaponComp->OnPlayerComboHit.Contains(this, FName("OnPlayerComboHit")))
	{
		WeaponComp->OnPlayerComboHit.AddDynamic(this, &ACEnemyCharacterBase::OnPlayerComboHit);
		UE_LOG(LogTemp, Log, TEXT("[Enemy] Successfully subscribed to player combo hits"));
	}
}

void ACEnemyCharacterBase::UnsubscribeFromPlayerComboHits()
{
	// 플레이어 찾기
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC) return;

	APawn* PlayerPawn = PC->GetPawn();
	if (!PlayerPawn) return;

	// WeaponComponent 찾아서 언바인딩
	UCPlayerWeaponComponent* WeaponComp = PlayerPawn->FindComponentByClass<UCPlayerWeaponComponent>();
	if (WeaponComp)
	{
		WeaponComp->OnPlayerComboHit.RemoveDynamic(this, &ACEnemyCharacterBase::OnPlayerComboHit);
		UE_LOG(LogTemp, Log, TEXT("[Enemy] Unsubscribed from player combo hits"));
	}
}

// ============================================================
// 공통 유틸리티 함수
// ============================================================

void ACEnemyCharacterBase::PlayMontageIfValid(UAnimMontage* Montage, float PlayRate) const
{
	if (!Montage)
		return;

	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
		{
			AnimInstance->Montage_Play(Montage, PlayRate);
		}
	}
}



void ACEnemyCharacterBase::OnPlayerComboHit(AActor* HitActor, int32 ComboIndex, float Damage)
{
	// 자신이 맞았는지 확인
	if (HitActor != this)
	{
		return;
	}
    
	const TCHAR* DirText = TEXT("None");
	switch (LastHitDirection)
	{
	case EEnemyHitDirection::FromLeft: DirText = TEXT("FromLeft"); break;
	case EEnemyHitDirection::FromRight: DirText = TEXT("FromRight"); break;
	default: break;
	}
	
	UE_LOG(LogTemp, Warning, TEXT("[%s] OnPlayerComboHit CALLED! Combo: %d, Damage: %.1f, Dir: %s (DotRight: %.3f)"),
		*GetName(), ComboIndex, Damage, DirText, LastHitDirectionRightDot);
    
	// 사망 상태면 무시
	if (State == EEnemyState::Dead)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] Ignoring hit - already dead"), *GetName());
		return;
	}
    
	// 콤보 인덱스 저장
	PlayerCurrentCombo = ComboIndex;
	StartHitShake();
    
	// 콤보에 맞는 피격 몽타주 재생
	FName MontageSource = NAME_None;
	UAnimMontage* MontageToPlay = ResolveComboHitReactionMontage(ComboIndex, MontageSource);
	
	if (!MontageToPlay)
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] No hit reaction montage for combo index %d! Dir: %s, Combo size: %d, ComboLeft size: %d, ComboRight size: %d, Hit size: %d, HitLeft size: %d, HitRight size: %d"),
			*GetName(), ComboIndex, DirText,
			ComboHitReactionMontages.Num(), ComboHitReactionMontagesLeft.Num(), ComboHitReactionMontagesRight.Num(),
			HitReactionMontages.Num(), HitReactionMontagesLeft.Num(), HitReactionMontagesRight.Num());
	}
    
	// 몽타주 재생
	if (MontageToPlay)
	{
		bIsHitStunned = true;
		StopMove();

		if (AAIController* AIC = Cast<AAIController>(GetController()))
		{
			AIC->StopMovement();
		}
        
		bDirectMoveActive = false;

		if (UCharacterMovementComponent* MovementComp = GetCharacterMovement())
		{
			MovementComp->StopMovementImmediately();
			MovementComp->Velocity = FVector::ZeroVector;
		}
    	
		if (USkeletalMeshComponent* MeshComp = GetMesh())
		{
			if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
			{
				// 이미 재생 중인 몽타주가 있으면 중단하고 새로 재생
				if (AnimInstance->Montage_IsPlaying(MontageToPlay))
				{
					AnimInstance->Montage_Stop(0.2f, MontageToPlay);
				}
                
				const float Duration = AnimInstance->Montage_Play(MontageToPlay, 1.0f);
                
				UE_LOG(LogTemp, Warning, TEXT("[%s] Playing combo %d hit reaction montage from %s (Duration: %.2f)"),
					*GetName(), ComboIndex, MontageSource.IsNone() ? TEXT("Unknown") : *MontageSource.ToString(), Duration);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[%s] AnimInstance is NULL!"), *GetName());
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[%s] MeshComp is NULL!"), *GetName());
		}
		

        
		// 피격 경직 타이머 설정
		GetWorldTimerManager().ClearTimer(HitStunTimer);
		GetWorldTimerManager().SetTimer(HitStunTimer, this, &ACEnemyCharacterBase::EndHitStun, HitStunDuration, false);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] MontageToPlay is NULL! Cannot play hit reaction"), *GetName());
	}
}

void ACEnemyCharacterBase::EndHitStun()
{
	if (State == EEnemyState::Dead)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[%s] EndHitStun ignored - already dead"), *GetName());
		return;
	}
	
	bIsHitStunned = false;
	UE_LOG(LogTemp, Verbose, TEXT("[%s] Hit stun ended"), *GetName());
}

void ACEnemyCharacterBase::SpawnDamageReceivedEffect(const FVector& HitLocation, const FVector& HitNormal)
{
	if (TakeHitEffect)
	{
		FRotator Rotation = HitNormal.Rotation();
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			TakeHitEffect,
			HitLocation,
			Rotation,
			FVector(1.0f),
			true,
			true,
			ENCPoolMethod::None);
	}
}

void ACEnemyCharacterBase::StartHitShake()
{
	if (bIsShaking)
	{
		StopHitShake();
	}

	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
		return;

	bIsShaking = true;
	HitShakeElapsed = 0.f;
	OriginalMeshLocation = MeshComp->GetRelativeLocation();

	GetWorldTimerManager().SetTimer(
		HitShakeTimer,
		this,
		&ACEnemyCharacterBase::UpdateHitShake,
		0.01f,  // 100fps로 업데이트
		true
	);

	if (bShowDebugInfo)
	{
		UE_LOG(LogTemp, Log, TEXT("[Enemy] Hit shake started"));
	}
}

void ACEnemyCharacterBase::UpdateHitShake()
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp || !bIsShaking)
	{
		StopHitShake();
		return;
	}

	HitShakeElapsed += 0.01f;

	// 지속 시간 종료
	if (HitShakeElapsed >= HitShakeDuration)
	{
		StopHitShake();
		return;
	}

	// 사인파로 위아래 흔들기
	const float Progress = HitShakeElapsed / HitShakeDuration;
	const float DecayFactor = 1.f - Progress;  // 점점 약해지는 효과
	const float ShakeAmount = FMath::Sin(HitShakeElapsed * HitShakeFrequency) * HitShakeIntensity * DecayFactor;
    
	FVector NewLocation = OriginalMeshLocation;
	NewLocation.Z += ShakeAmount;
    
	MeshComp->SetRelativeLocation(NewLocation);
}

void ACEnemyCharacterBase::StopHitShake()
{
	if (!bIsShaking)
		return;

	bIsShaking = false;
	GetWorldTimerManager().ClearTimer(HitShakeTimer);

	// 원래 위치로 복귀
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetRelativeLocation(OriginalMeshLocation);
	}

	if (bShowDebugInfo)
	{
		UE_LOG(LogTemp, Log, TEXT("[Enemy] Hit shake stopped"));
	}
}

// ============================================================
// 사운드 처리
// ============================================================

void ACEnemyCharacterBase::CacheSoundsFromDataAsset()
{
	// Base 클래스는 비워두고 자식 클래스에서 오버라이드
}

void ACEnemyCharacterBase::PlayEnemySound(const TWeakObjectPtr<USoundBase>& Sound, float VolumeMultiplier)
{
    if (!Sound.IsValid())
		return;
        
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UCSoundManagerSubsystem* SoundMgr = GI->GetSubsystem<UCSoundManagerSubsystem>())
		{
			SoundMgr->PlaySFX3D(Sound.Get(), GetActorLocation(), VolumeMultiplier);
		}
	}
}