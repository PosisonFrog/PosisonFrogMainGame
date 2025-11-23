#include "CBossPattern_Barrage.h"
#include "00_Character/01_Enemy/CEnemyBossCharacter.h"
#include "00_Character/02_Component/01_EnemyComponent/CEnemyWeaponComponent.h"
#include "00_Character/01_Enemy/01_AIController/BossAIController.h"
#include "AIController.h"
#include "00_Character/01_Enemy/02_Weapon/CBossProjectile.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/PrimitiveComponent.h"

UCBossPattern_Barrage::UCBossPattern_Barrage()
{
	PatternId = FName("Barrage");
	CurrentMaxShots = 15;
	PrimaryComponentTick.bCanEverTick = false;
}

bool UCBossPattern_Barrage::ExecutePattern(int32 PhaseIndex, const FBossPatternDefinition& PatternData)
{
	Super::ExecutePattern(PhaseIndex, PatternData);


	//-------------데이터 에셋 연동----------------
	
	if (PatternData.ProjectileRain.ProjectileClass)
	{
		ProjectileClass = PatternData.ProjectileRain.ProjectileClass;
	}

	if (PatternData.ProjectileRain.SpawnRadius > 0.f)
	{
		RandomSpawnRadius = PatternData.ProjectileRain.SpawnRadius;
	}
    
	if (PatternData.ProjectileRain.SpawnHeight > 0.f)
	{
		DropHeight = PatternData.ProjectileRain.SpawnHeight;
	}

	if (FMath::Abs(PatternData.ProjectileRain.InitialVelocity.Z) > 0.f)
	{
		FallSpeed = FMath::Abs(PatternData.ProjectileRain.InitialVelocity.Z);
	}
	
	if (PatternData.ProjectileRain.SpawnInterval > 0.f)
	{
		ShotInterval = PatternData.ProjectileRain.SpawnInterval;
	}

	
	UE_LOG(LogTemp, Warning, TEXT("[Barrage] Executing barrage attack - Phase %d"), PhaseIndex);
	UE_LOG(LogTemp, Log, TEXT("[Barrage] ExecutionTime: %.2f, RecoveryTime: %.2f, Cooldown: %.2f"), 
		PatternData.ExecutionTime, PatternData.RecoveryTime, PatternData.Cooldown);

	if (!OwnerBoss.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[Barrage] Invalid OwnerBoss"));
		return false;
	}

	CurrentPatternData = PatternData;
	
	CurrentMaxShots = FMath::Max(1, FMath::FloorToInt(PatternData.ExecutionTime / ShotInterval));
	UE_LOG(LogTemp, Log, TEXT("[Barrage] Calculated Max Shots: %d"), CurrentMaxShots);

	if (ABossAIController* BossAI = Cast<ABossAIController>(GetBossAI()))
	{
		BossAI->SetChaseEnabled(false);
		UE_LOG(LogTemp, Log, TEXT("[Barrage] Disabled chase for Barrage"));
	}

	PrePlannedDropLocations.Empty();
	if (AActor* Player = GetPlayerTarget())
	{
		const FVector PlayerLocation = Player->GetActorLocation();
		UWorld* World = GetWorld();

		for (int32 i = 0; i < CurrentMaxShots; ++i)
		{
			const FVector2D RandomOffset = FMath::RandPointInCircle(RandomSpawnRadius);
			FVector TraceStart = PlayerLocation + FVector(RandomOffset.X, RandomOffset.Y, 0.0f);
			
			TraceStart.Z += 500.0f; 
			FVector TraceEnd = TraceStart - FVector(0.0f, 0.0f, 2000.0f);
            
			FHitResult HitResult;
			FCollisionQueryParams Params;
			Params.AddIgnoredActor(Player); 
			Params.AddIgnoredActor(OwnerBoss.Get()); 

			FVector FinalDropLocation = TraceStart; // 기본값

			// ECC_Visibility 또는 ECC_WorldStatic 채널로 바닥 검출
			if (World && World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_WorldStatic, Params))
			{
				FinalDropLocation = HitResult.ImpactPoint;
				FinalDropLocation.Z += 5.0f; 
			}
			else
			{
				// 못찾으면 플레이어 발밑.
				FinalDropLocation = PlayerLocation + FVector(RandomOffset.X, RandomOffset.Y, -88.0f); 
			}

			PrePlannedDropLocations.Add(FinalDropLocation);
			UE_LOG(LogTemp, Log, TEXT("[Barrage] Planned drop location %d: %s"), i, *FinalDropLocation.ToString());
		}
	}

	StartBarrage();

	return true;
}

void UCBossPattern_Barrage::OnPatternEnd()
{
	Super::OnPatternEnd();
	ClearAllTimers();
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
	if(GetWorld()) GetWorld()->GetTimerManager().ClearTimer(TH_FinishDelay);

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

void UCBossPattern_Barrage::StartBarrage()
{
	if (!OwnerBoss.IsValid() || !OwnerBoss->GetWorld())
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Barrage] Starting barrage!"));

	BarrageShotCount = 0;

	if (BarrageMontage)
	{
		PlayMontage(BarrageMontage);
	}

	OwnerBoss->GetWorld()->GetTimerManager().SetTimer(
		BarrageLoopTimer,
		this,
		&UCBossPattern_Barrage::FireBarrageShot,
		ShotInterval,
		true
	);
}

void UCBossPattern_Barrage::FireBarrageShot()
{
	if (BarrageShotCount >= CurrentMaxShots)
	{
		if (GetWorld())
		{
			GetWorld()->GetTimerManager().ClearTimer(BarrageLoopTimer);
			UE_LOG(LogTemp, Warning, TEXT("[Barrage] All shots fired (%d/%d). Barrage loop stopped."), BarrageShotCount, CurrentMaxShots);

			/*GetWorld()->GetTimerManager().SetTimer(
				TH_FinishDelay, 
				this, 
				&UCBossPattern_Barrage::FinishBarrage, 
				CurrentPatternData.RecoveryTime, 
				false
			);*/
			FinishBarrage();
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
			if (ACBossProjectile* BossProjectile = Cast<ACBossProjectile>(Projectile))
			{
				BossProjectile->InitProjectile(
					SafeBossPtr, 
					SafeThis->BarrageDamage, 
					SafeThis->FallSpeed, 
					FVector::DownVector 
				);
			}
			else 
			{
				FVector InitialVelocity = FVector(0.0f, 0.0f, -SafeThis->FallSpeed);
				if (UProjectileMovementComponent* ProjectileMovement = Projectile->FindComponentByClass<UProjectileMovementComponent>())
				{
					ProjectileMovement->Velocity = InitialVelocity;
				}
			}
		}
	});

	World->GetTimerManager().SetTimer(NewCoconutTimer, CoconutDelegate, CoconutFallDelay, false);
	CoconutSpawnTimers.Add(NewCoconutTimer);

	BarrageShotCount++;
	UE_LOG(LogTemp, Log, TEXT("[Barrage] Fired shot %d/%d"), BarrageShotCount, CurrentMaxShots);
}

void UCBossPattern_Barrage::FinishBarrage()
{
	FinishPattern(true);
}