// Fill out your copyright notice in the Description page of Project Settings.


#include "00_Character/01_Enemy/CEnemyCharacter.h"

#include "01_Item/CHealOrb.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

ACEnemyCharacter::ACEnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = PatrolMoveSpeed;
		MoveComp->bOrientRotationToMovement = true;
		MoveComp->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
		MoveComp->MaxStepHeight = 45.0f;
		MoveComp->BrakingDecelerationWalking = 2000.0f;
	}
}

void ACEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	CurrentHP = MaxHP;
	HomeLocation = GetActorLocation();
	Player = Cast<ACPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));

	// 순찰 시작 목표 보장
	EnsurePatrolGoal();

	EnterState(EEnemyState::Patrol);
}

void ACEnemyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bIsDead) return;
	if (!IsPlayerValid()) return;

	if (!ShouldUpdateAI(DeltaTime)) return;

	UpdateFSM(DeltaTime);
}

float ACEnemyCharacter::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
                                   class AController* EventInstigator, AActor* DamageCauser)
{
	if (bIsDead || DamageAmount <= 0.f)
		return 0.f;

	// 부위/타입 무시: 항상 Body 판정으로 단일 처리
	CurrentHP = FMath::Clamp(CurrentHP - DamageAmount, 0.f, MaxHP);

	if (CurrentHP <= 0.f)
		Die();

	return DamageAmount;
}

void ACEnemyCharacter::UpdateFSM(float DeltaTime)
{
	const FVector MyLoc = GetActorLocation();
	const FVector HisLoc = Player->GetActorLocation();
	const float Dist = FVector::Dist2D(MyLoc, HisLoc);
	const bool  bLOS = HasSightToPlayer();

	// Leash: 원점에서 너무 멀리 벗어났으면 복귀 강제
	const float FromHome = FVector::Dist2D(MyLoc, HomeLocation);
	if (State != EEnemyState::Dead && FromHome >= LeashMaxDistance)
	{
	    EnterState(EEnemyState::ReturnHome);
	}

	switch (State)
	{
	case EEnemyState::Patrol:
	{
	    // 순찰 이동
	    MoveTowards(CurrentPatrolGoal, PatrolMoveSpeed);
	    if (Reached(CurrentPatrolGoal, PatrolPointReachRadius))
	    {
	        PatrolWaitAcc += DeltaTime;
	        if (PatrolWaitAcc >= PatrolWaitTime)
	        {
	            PatrolWaitAcc = 0.f;
	            EnsurePatrolGoal(); // 다음 순찰 목표
	        }
	    }

	    // 인지 → 경계 진입
	    if (Dist <= ChaseStartDistance && bLOS)
	        EnterState(EEnemyState::Alert);
	    break;
	}

	case EEnemyState::Alert:
	    AlertAcc += DeltaTime;
	    if (AlertAcc >= AlertDuration)
	        EnterState(EEnemyState::Chase);
	    break;

	case EEnemyState::Chase:
	{
	    // 공격 진입
	    if (Dist <= AttackEnterDistance)
	    {
	        EnterState(EEnemyState::Attack);
	        break;
	    }

	    // LoS가 끊기면 유예 시간을 누적, 넘으면 추격 해제
	    if (!bLOS)
	    {
	        LoseSightAcc += DeltaTime;
	        if (LoseSightAcc >= LoseSightGrace)
	        {
	            EnterState(EEnemyState::ReturnHome);
	            break;
	        }
	    }
	    else
	    {
	        LoseSightAcc = 0.f;
	    }

	    // 너무 멀어지면 추격 포기
	    if (Dist >= ChaseStopDistance)
	    {
	        EnterState(EEnemyState::ReturnHome);
	        break;
	    }

	    // 단순 추격(나중에 MoveTo로 교체 가능)
	    MoveTowards(HisLoc, ChaseMoveSpeed);
	    break;
	}

	case EEnemyState::Attack:
	{
	    // 공격 범위 이탈 → 다시 추격
	    if (Dist > AttackEnterDistance * 1.3f)
	    {
	        EnterState(EEnemyState::Chase);
	        break;
	    }

	    DoMeleeHit(); // 1회 스윕 + 쿨다운 보호
	    break;
	}

	case EEnemyState::ReturnHome:
	{
	    MoveTowards(HomeLocation, PatrolMoveSpeed);
	    if (Reached(HomeLocation, ReturnHomeReachDist))
	    {
	        EnterState(EEnemyState::Patrol);
	    }
	    break;
	}

	case EEnemyState::Dead:
	    break;
	}
}

void ACEnemyCharacter::EnterState(EEnemyState NewState)
{
	State = NewState;

	switch (State)
	{
	case EEnemyState::Patrol:
		AlertAcc = 0.f;
		LoseSightAcc = 0.f;
		if (UCharacterMovementComponent* Move = GetCharacterMovement())
			Move->MaxWalkSpeed = PatrolMoveSpeed;
		EnsurePatrolGoal();
		break;

	case EEnemyState::Alert:
		AlertAcc = 0.f;
		if (UCharacterMovementComponent* Move = GetCharacterMovement())
			Move->StopMovementImmediately();
		break;

	case EEnemyState::Chase:
		LoseSightAcc = 0.f;
		if (UCharacterMovementComponent* Move = GetCharacterMovement())
			Move->MaxWalkSpeed = ChaseMoveSpeed;
		break;

	case EEnemyState::Attack:
		// 실제 타격은 DoMeleeHit()에서 1회 스윕 + 쿨다운
		break;

	case EEnemyState::ReturnHome:
		if (UCharacterMovementComponent* Move = GetCharacterMovement())
			Move->MaxWalkSpeed = PatrolMoveSpeed;
		break;

	case EEnemyState::Dead:
		break;
	}
}

bool ACEnemyCharacter::ShouldUpdateAI(float DeltaTime)
{
	const float Dist = FVector::Dist2D(GetActorLocation(), Player->GetActorLocation());
	if (Dist <= NearThinkDistance)
		return true;

	CheapAcc += DeltaTime;
	if (CheapAcc >= CheapThinkInterval)
	{
		CheapAcc = 0.f;
		return true;
	}
	return false;
}

bool ACEnemyCharacter::HasSightToPlayer() const
{
	if (!IsPlayerValid()) return false;

	const FVector MyLoc = GetActorLocation();
	const FVector HisLoc = Player->GetActorLocation();
	const float Dist = FVector::Dist2D(MyLoc, HisLoc);

	if (Dist <= EasyChaseDistance)
		return true;

	FHitResult Hit;
	FCollisionQueryParams P(SCENE_QUERY_STAT(EnemySight), false, this);
	P.AddIgnoredActor(this);

	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, MyLoc, HisLoc, ECC_Visibility, P);
	return (!bHit || Hit.GetActor() == Player);
}

void ACEnemyCharacter::MoveTowards(const FVector& Dest, float DesiredSpeed)
{
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
		Move->MaxWalkSpeed = DesiredSpeed;

	const FVector Dir = (Dest - GetActorLocation()).GetSafeNormal2D();
	if (!Dir.IsNearlyZero())
	{
		AddMovementInput(Dir, 1.f);
		SetActorRotation(Dir.Rotation());
	}
}

void ACEnemyCharacter::EnsurePatrolGoal()
{
	if (PatrolPoints.Num() > 0)
	{
		// 고정 경로: 순차 순환
		CurrentPatrolGoal = PatrolPoints[PatrolIndex % PatrolPoints.Num()];
		PatrolIndex = (PatrolIndex + 1) % PatrolPoints.Num();
	}
	else if (bRandomPatrolAroundHome)
	{
		PickNewRoamGoal();
	}
	else
	{
		CurrentPatrolGoal = HomeLocation;
	}
}

void ACEnemyCharacter::PickNewRoamGoal()
{
	// 원점 근방 랜덤 목표(에디터 세팅 없이 순찰 가능)
	const float Angle = FMath::FRandRange(0.f, 2 * PI);
	const float Rad = FMath::FRandRange(PatrolRoamRadius * 0.4f, PatrolRoamRadius);
	const FVector Offset = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Rad;
	CurrentPatrolGoal = HomeLocation + Offset;
}

bool ACEnemyCharacter::Reached(const FVector& Goal, float Radius) const
{
	return FVector::Dist2D(GetActorLocation(), Goal) <= Radius;
}

void ACEnemyCharacter::DoMeleeHit()
{
	if (!CanAttack()) return;

	// 플레이어 방향으로 살짝 정렬
	const FVector ToPlayer = (Player->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	if (!ToPlayer.IsNearlyZero())
		SetActorRotation(ToPlayer.Rotation());

	const FVector Origin = GetActorLocation() + GetActorForwardVector() * AttackOffset;
	const FVector End = Origin + GetActorForwardVector() * AttackRange;
	const FCollisionShape Shape = FCollisionShape::MakeSphere(AttackRadius);

	FHitResult Hit;
	FCollisionQueryParams P(SCENE_QUERY_STAT(EnemyMelee), false, this);
	P.AddIgnoredActor(this);

	if (GetWorld()->SweepSingleByChannel(Hit, Origin, End, FQuat::Identity, ECC_Pawn, Shape, P))
	{
		if (AActor* Other = Hit.GetActor())
		{
			if (Other->IsA(ACPlayerCharacter::StaticClass()))
			{
				UGameplayStatics::ApplyDamage(Other, AttackDamage, GetController(), this, UDamageType::StaticClass());
			}
		}
	}

#if !(UE_BUILD_SHIPPING)
	// 필요 시 디버그
	DrawDebugLine(GetWorld(), Origin, End, FColor::Yellow, false, 0.15f, 0, 1.5f);
	DrawDebugSphere(GetWorld(), End, AttackRadius, 16, FColor::Yellow, false, 0.15f, 0, 1.5f);
#endif
	LastAttackTime = GetWorld()->TimeSeconds;
}

bool ACEnemyCharacter::CanAttack() const
{
	return (GetWorld()->TimeSeconds - LastAttackTime) >= AttackCooldown;
}

void ACEnemyCharacter::Die()
{
	if (bIsDead) return;
	bIsDead = true;

	SetCanBeDamaged(false);

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}
	DetachFromControllerPendingDestroy();

	EnterState(EEnemyState::Dead);

	// 힐 오브 드랍(풀 우선)
	/*const FVector SpawnLoc = GetActorLocation() + FVector(0, 0, 30.f);
	const FRotator SpawnRot = FRotator::ZeroRotator;
	ACPlayerCharacter* PC = Cast<ACPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));

	if (UHealOrbPoolSubsystem* Pool = GetWorld()->GetSubsystem<UHealOrbPoolSubsystem>())
	{
		Pool->Acquire(SpawnLoc, SpawnRot, PC);
	}
	else
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		if (ACHealOrb* Orb = GetWorld()->SpawnActor<ACHealOrb>(ACHealOrb::StaticClass(), SpawnLoc, SpawnRot, Params))
			Orb->ForceSetTarget(PC);
	}*/

	SetLifeSpan(5.f);
}
