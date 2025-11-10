#include "CBossPattern_Barrage.h"
#include "00_Character/01_Enemy/CEnemyBossCharacter.h"
#include "00_Character/02_Component/01_EnemyComponent/CEnemyWeaponComponent.h"
#include "00_Character/01_Enemy/01_AIController/BossAIController.h"
#include "AIController.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/PrimitiveComponent.h"

UCBossPattern_Barrage::UCBossPattern_Barrage()
{
	PatternId = FName("Barrage");

	CurrentMaxShots = Phase1_ShotCount;
	CurrentWarnDuration = Phase1_WarnDuration;
	CurrentRecoveryDuration = Phase1_RecoveryDuration;
}

void UCBossPattern_Barrage::BeginDestroy()
{
	UE_LOG(LogTemp, Log, TEXT("[Barrage] BeginDestroy called"));
	ClearAllTimers();
	Super::BeginDestroy();
}

void UCBossPattern_Barrage::ExecutePattern(int32 PhaseIndex)
{
	Super::ExecutePattern(PhaseIndex);

	UE_LOG(LogTemp, Warning, TEXT("[Barrage] Executing barrage attack - Phase %d"), PhaseIndex);
	UE_LOG(LogTemp, Log, TEXT("[Barrage] Max Shots: %d, Warn: %.2fs, Recovery: %.2fs"), 
		   CurrentMaxShots, CurrentWarnDuration, CurrentRecoveryDuration);

	if (!OwnerBoss.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[Barrage] Invalid OwnerBoss"));
		return;
	}

	// Chase 비활성화
	if (ABossAIController* BossAI = Cast<ABossAIController>(GetBossAI()))
	{
		BossAI->SetChaseEnabled(false);
		UE_LOG(LogTemp, Log, TEXT("[Barrage] Disabled chase for Barrage"));
	}

	// 낙하 위치 미리 계획
	PrePlannedDropLocations.Empty();
	if (AActor* Player = GetPlayerTarget())
	{
		const FVector PlayerLocation = Player->GetActorLocation();
		
		for (int32 i = 0; i < CurrentMaxShots; ++i)
		{
			const FVector2D RandomOffset = FMath::RandPointInCircle(RandomSpawnRadius);
			FVector DropLocation = PlayerLocation + FVector(RandomOffset.X, RandomOffset.Y, 0.0f);
			PrePlannedDropLocations.Add(DropLocation);

			UE_LOG(LogTemp, Log, TEXT("[Barrage] Planned drop location %d: %s"), i, *DropLocation.ToString());
		}
	}

	StartBarrage();
}

void UCBossPattern_Barrage::OnPatternEnd()
{
	Super::OnPatternEnd();

	// Chase 재활성화
	if (ABossAIController* BossAI = Cast<ABossAIController>(GetBossAI()))
	{
		BossAI->SetChaseEnabled(true);
		UE_LOG(LogTemp, Log, TEXT("[Barrage] Re-enabled chase after Barrage"));
	}

	PrePlannedDropLocations.Empty();
	BarrageShotCount = 0;

	UE_LOG(LogTemp, Log, TEXT("[Barrage] Pattern ended"));
}

void UCBossPattern_Barrage::Cleanup()
{
	Super::Cleanup();
	ClearAllTimers();
	PrePlannedDropLocations.Empty();
	BarrageShotCount = 0;
}

void UCBossPattern_Barrage::ClearAllTimers()
{
	if (!GetWorld()) return;

	FTimerManager& TimerManager = GetWorld()->GetTimerManager();
	TimerManager.ClearTimer(BarrageLoopTimer);
	
	for (FTimerHandle& Timer : CoconutSpawnTimers)
	{
		TimerManager.ClearTimer(Timer);
	}
	CoconutSpawnTimers.Empty();
}

void UCBossPattern_Barrage::UpdatePhaseSettings(int32 PhaseIndex)
{
	Super::UpdatePhaseSettings(PhaseIndex);

	if (PhaseIndex == 0)
	{
		CurrentMaxShots = Phase1_ShotCount;
		CurrentWarnDuration = Phase1_WarnDuration;
		CurrentRecoveryDuration = Phase1_RecoveryDuration;
		UE_LOG(LogTemp, Log, TEXT("[Barrage] Updated to Phase 1 settings (Shots: %d)"), CurrentMaxShots);
	}
	else if (PhaseIndex >= 1)
	{
		CurrentMaxShots = Phase2_ShotCount;
		CurrentWarnDuration = Phase2_WarnDuration;
		CurrentRecoveryDuration = Phase2_RecoveryDuration;
		UE_LOG(LogTemp, Log, TEXT("[Barrage] Updated to Phase 2 settings (Shots: %d)"), CurrentMaxShots);
	}
}

void UCBossPattern_Barrage::StartBarrage()
{
	if (!OwnerBoss.IsValid() || !OwnerBoss->GetWorld())
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Barrage] Starting barrage!"));

	BarrageShotCount = 0;

	// 애니메이션 재생
	if (BarrageMontage)
	{
		PlayMontage(BarrageMontage);
	}

	// 반복 발사 타이머 시작
	OwnerBoss->GetWorld()->GetTimerManager().SetTimer(
		BarrageLoopTimer,
		this,
		&UCBossPattern_Barrage::FireBarrageShot,
		ShotInterval,
		true
	);

	// 전체 지속시간 타이머를 제거하여 발사 횟수에만 의존하도록 함
}

void UCBossPattern_Barrage::FireBarrageShot()
{
	// 발사 횟수를 먼저 체크해서 모든 발사를 완료했으면 타이머를 중지
	if (BarrageShotCount >= CurrentMaxShots)
	{
		if (GetWorld())
		{
			GetWorld()->GetTimerManager().ClearTimer(BarrageLoopTimer);
			UE_LOG(LogTemp, Warning, TEXT("[Barrage] All shots fired (%d/%d). Barrage loop stopped."), BarrageShotCount, CurrentMaxShots);
		}
		return;
	}

	if (!OwnerBoss.IsValid() || !ProjectileClass || !PrePlannedDropLocations.IsValidIndex(BarrageShotCount))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Barrage] Invalid state in FireBarrageShot, stopping barrage loop."));
		if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(BarrageLoopTimer);
		return;
	}

	UWorld* World = OwnerBoss->GetWorld();
	if (!World)
	{
		return;
	}

	ACEnemyBossCharacter* BossPtr = OwnerBoss.Get();
	if (!BossPtr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Barrage] BossPtr is null in FireBarrageShot, stopping barrage loop."));
		World->GetTimerManager().ClearTimer(BarrageLoopTimer);
		return;
	}

	const FVector TargetDropLocation = PrePlannedDropLocations[BarrageShotCount];

	// 데칼 스폰
	if (WarningDecalClass)
	{
		FActorSpawnParameters DecalParams;
		DecalParams.Owner = BossPtr;
		DecalParams.Instigator = BossPtr;

		FTransform DecalTransform;
		DecalTransform.SetLocation(TargetDropLocation);
		DecalTransform.SetScale3D(FVector(WarningDecalRadius / 100.0f));

		if (AActor* Decal = World->SpawnActor<AActor>(WarningDecalClass, DecalTransform, DecalParams))
		{
			Decal->SetLifeSpan(CoconutFallDelay);
		}
	}

	// CoconutFallDelay 후 코코넛 스폰
	TWeakObjectPtr<UCBossPattern_Barrage> WeakThis(this);
	TWeakObjectPtr<ACEnemyBossCharacter> WeakBoss = OwnerBoss;
	TWeakObjectPtr<UWorld> WeakWorld(World);
	TSubclassOf<AActor> ProjectileClassCopy = ProjectileClass;
	
	FTimerHandle NewCoconutTimer;
	FTimerDelegate CoconutDelegate;
	CoconutDelegate.BindLambda([WeakThis, WeakWorld, WeakBoss, TargetDropLocation, ProjectileClassCopy]()
	{
		if (!WeakThis.IsValid() || !WeakWorld.IsValid() || !WeakBoss.IsValid() || !ProjectileClassCopy)
		{
			return;
		}

		UWorld* SafeWorld = WeakWorld.Get();
		ACEnemyBossCharacter* SafeBossPtr = WeakBoss.Get();
		UCBossPattern_Barrage* SafeThis = WeakThis.Get();

		FVector SpawnLocation = TargetDropLocation + FVector(0.0f, 0.0f, SafeThis->DropHeight);

		FActorSpawnParameters ProjectileParams;
		ProjectileParams.Owner = SafeBossPtr;
		ProjectileParams.Instigator = SafeBossPtr;

		FTransform ProjectileTransform;
		ProjectileTransform.SetLocation(SpawnLocation);
		ProjectileTransform.SetRotation(FRotator(-90.0f, 0.0f, 0.0f).Quaternion());

		if (AActor* Projectile = SafeWorld->SpawnActor<AActor>(ProjectileClassCopy, ProjectileTransform, ProjectileParams))
		{
			FVector InitialVelocity = FVector(0.0f, 0.0f, -SafeThis->FallSpeed);
			if (UProjectileMovementComponent* ProjectileMovement = Projectile->FindComponentByClass<UProjectileMovementComponent>())
			{
				ProjectileMovement->Velocity = InitialVelocity;
			}
			else if (UPrimitiveComponent* PrimitiveComp = Cast<UPrimitiveComponent>(Projectile->GetRootComponent()))
			{
				if (PrimitiveComp->IsSimulatingPhysics())
				{
					PrimitiveComp->SetPhysicsLinearVelocity(InitialVelocity);
				}
			}
		}
	});

	World->GetTimerManager().SetTimer(NewCoconutTimer, CoconutDelegate, CoconutFallDelay, false);
	CoconutSpawnTimers.Add(NewCoconutTimer);

	// 발사 후 카운트 증가
	BarrageShotCount++;
	UE_LOG(LogTemp, Log, TEXT("[Barrage] Fired shot %d/%d"), BarrageShotCount, CurrentMaxShots);
}