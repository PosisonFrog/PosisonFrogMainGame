#include "CBossPatternManager.h"
#include "00_Character/01_Enemy/CEnemyBossCharacter.h"
#include "00_Character/02_Component/01_EnemyComponent/CEnemyBossPhaseComponent.h"
#include "00_Character/02_Component/01_EnemyComponent/CEnemyWeaponComponent.h"
#include "00_Character/01_Enemy/01_AIController/BossAIController.h"

#include "CBossPatternBase.h"
#include "00_BossPattern/CBossPattern_BasicAttack.h"
#include "00_BossPattern/CBossPattern_Rush.h"
#include "00_BossPattern/CBossPattern_Slam.h"
#include "00_BossPattern/CBossPattern_Barrage.h"

#include "AIController.h"
#include "Camera/CameraShakeBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/PrimitiveComponent.h"
#include "Sound/SoundBase.h"

UCBossPatternManager::UCBossPatternManager()
{
	PrimaryComponentTick.bCanEverTick = true;
	bIsPatternActive = false;

	BasicAttackPattern = CreateDefaultSubobject<UCBossPattern_BasicAttack>(TEXT("BasicAttackPattern"));
	BarragePattern = CreateDefaultSubobject<UCBossPattern_Barrage>(TEXT("BarragePattern"));
	RushPattern = CreateDefaultSubobject<UCBossPattern_Rush>(TEXT("RushPattern"));
	SlamPattern = CreateDefaultSubobject<UCBossPattern_Slam>(TEXT("SlamPattern"));
	
	PatternMap.Add(FName("BasicAttack"), BasicAttackPattern);
	PatternMap.Add(FName("Barrage"), BarragePattern);
	PatternMap.Add(FName("Rush"), RushPattern);
	PatternMap.Add(FName("Slam"), SlamPattern);
}

void UCBossPatternManager::BeginPlay()
{
	Super::BeginPlay();

	OwnerBoss = Cast<ACEnemyBossCharacter>(GetOwner());
	if (!OwnerBoss)
	{
		UE_LOG(LogTemp, Error, TEXT("[PatternManager] Owner is not CEnemyBossCharacter!"));
		PrimaryComponentTick.SetTickFunctionEnable(false);
		return;
	}

	PhaseComponent = OwnerBoss->FindComponentByClass<UCEnemyBossPhaseComponent>();
	WeaponComponent = OwnerBoss->FindComponentByClass<UCEnemyWeaponComponent>();
	
	if (!PhaseComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("[PatternManager] BossPhaseComponent not found!"));
		return;
	}

	InitializePatterns();
	BindToBossPhaseComponent();
}

void UCBossPatternManager::InitializePatterns()
{
	if (!OwnerBoss) return;
    
	for (auto& Pair : PatternMap)
	{
		if (Pair.Value)
		{
			Pair.Value->Initialize(OwnerBoss, WeaponComponent);
		}
	}
}

void UCBossPatternManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CleanupAllPatterns();
	CleanupUtilitySpawnTimers();
	CleanupMinionSpawnTimers();
	CleanupPatternActors();

	if (bProjectileRainActive)
	{
		StopProjectileRain(false);
	}

	UnbindFromBossPhaseComponent();
	
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PhaseTransitionTimer);
		World->GetTimerManager().ClearTimer(RushTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void UCBossPatternManager::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (UCBossPattern_Rush* LocalRushPattern = Cast<UCBossPattern_Rush>(FindPattern(FName("Rush"))))
	{
		if (LocalRushPattern->IsRushing())
		{
			LocalRushPattern->TickRushMovement(DeltaTime);
		}
	}
}

// ========================================
// Delegate Binding
// ========================================

void UCBossPatternManager::BindToBossPhaseComponent()
{
	if (!PhaseComponent) return;

	PhaseComponent->OnPhaseChanged.AddDynamic(this, &UCBossPatternManager::HandlePhaseChanged);
	PhaseComponent->OnPatternStarted.AddDynamic(this, &UCBossPatternManager::HandlePatternStarted);
	PhaseComponent->OnPatternFinished.AddDynamic(this, &UCBossPatternManager::HandlePatternFinished);
	PhaseComponent->OnShoutStarted.AddDynamic(this, &UCBossPatternManager::HandleShoutStarted);
}

void UCBossPatternManager::UnbindFromBossPhaseComponent()
{
	if (!PhaseComponent) return;

	PhaseComponent->OnPhaseChanged.RemoveDynamic(this, &UCBossPatternManager::HandlePhaseChanged);
	PhaseComponent->OnPatternStarted.RemoveDynamic(this, &UCBossPatternManager::HandlePatternStarted);
	PhaseComponent->OnPatternFinished.RemoveDynamic(this, &UCBossPatternManager::HandlePatternFinished);
	PhaseComponent->OnShoutStarted.RemoveDynamic(this, &UCBossPatternManager::HandleShoutStarted);
}

// ========================================
// Event Handlers
// ========================================

void UCBossPatternManager::HandlePhaseChanged(int32 PhaseIndex, const FBossPhaseDefinition& PhaseData)
{
	PlayPhaseTransition(PhaseIndex);
	UpdatePhaseStats(PhaseIndex);

	for (const auto& Pair : PatternMap)
	{
		if (UCBossPatternBase* Pattern = Pair.Value)
		{
			Pattern->UpdatePhaseSettings(PhaseIndex);
		}
	}
}

void UCBossPatternManager::HandlePatternStarted(int32 PhaseIndex, FName PatternId, const FBossPatternDefinition& PatternData, float RemainingPower)
{
	if (!PhaseComponent || !PhaseComponent->IsBattleStarted()) return;

	// 거리 기반 패턴 검증
	float DistanceToPlayer = GetDistanceToPlayer();
	if (!ValidatePatternDistance(PatternId, DistanceToPlayer))
	{
		FName FallbackPatternId = GetFallbackPattern(PatternId, DistanceToPlayer);
		UE_LOG(LogTemp, Warning, TEXT("[PatternManager] Pattern %s invalid at distance %.1f, using fallback: %s"), 
			*PatternId.ToString(), DistanceToPlayer, *FallbackPatternId.ToString());
		PatternId = FallbackPatternId;
	}

	CurrentPatternId = PatternId;
	bIsPatternActive = true;

	CleanupPatternActors();
	CleanupUtilitySpawnTimers();
	CleanupMinionSpawnTimers();
	StopProjectileRain(false);

	SpawnPatternActors(PatternData);

	if (UCBossPatternBase* Pattern = FindPattern(PatternId))
	{
		Pattern->ExecutePattern(PhaseIndex);
	}
	else
	{
		if (UCBossPatternBase* BasicAttack = FindPattern(FName("BasicAttack")))
		{
			BasicAttack->ExecutePattern(PhaseIndex);
		}
	}
}

void UCBossPatternManager::HandlePatternFinished(int32 PhaseIndex, FName PatternId, const FBossPatternDefinition& PatternData, float RemainingPower)
{
	bIsPatternActive = false;
	CleanupUtilitySpawnTimers();
	CleanupMinionSpawnTimers();
	CleanupPatternActors();

	if (UCBossPatternBase* Pattern = FindPattern(PatternId))
	{
		Pattern->OnPatternEnd();
	}
}

void UCBossPatternManager::HandleShoutStarted(int32 PhaseIndex, FName ShoutId, float Duration)
{
}

// ========================================
// Rush Pattern Control
// ========================================

void UCBossPatternManager::HandleRushMovementStart()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(RushTimerHandle);
	}
	
	UCBossPattern_Rush* LocalRushPattern = Cast<UCBossPattern_Rush>(FindPattern(FName("Rush")));
	if (LocalRushPattern)
	{
		LocalRushPattern->HandleRushMovementStart();
		UE_LOG(LogTemp, Log, TEXT("[PatternManager] Rush movement started"));

		float RushDuration = 3.0f; 
		GetWorld()->GetTimerManager().SetTimer(RushTimerHandle, this, &UCBossPatternManager::HandleRushMovementStop, RushDuration, false);
	}
}

void UCBossPatternManager::HandleRushMovementStop()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(RushTimerHandle);
	}
	
	UCBossPattern_Rush* LocalRushPattern = Cast<UCBossPattern_Rush>(FindPattern(FName("Rush")));
	if (LocalRushPattern)
	{
		LocalRushPattern->HandleRushMovementStop();
		UE_LOG(LogTemp, Log, TEXT("[PatternManager] Rush movement stopped"));
	}
}

// ========================================
// Utilities
// ========================================

AActor* UCBossPatternManager::GetPlayerTarget() const
{
	return UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
}

AAIController* UCBossPatternManager::GetBossAI() const
{
	if (!OwnerBoss) return nullptr;
	return Cast<AAIController>(OwnerBoss->GetController());
}

float UCBossPatternManager::GetDistanceToPlayer() const
{
	if (!OwnerBoss) return FLT_MAX;
	
	AActor* Player = GetPlayerTarget();
	if (!Player) return FLT_MAX;
	
	return FVector::Dist2D(OwnerBoss->GetActorLocation(), Player->GetActorLocation());
}

bool UCBossPatternManager::ValidatePatternDistance(FName PatternId, float Distance) const
{
	// BasicAttack, Slam: 근거리만
	if (PatternId == FName("BasicAttack") || PatternId == FName("Slam"))
	{
		return Distance <= CloseRangeThreshold;
	}
	
	// Barrage: 중거리만
	if (PatternId == FName("Barrage"))
	{
		return Distance > CloseRangeThreshold && Distance <= MidRangeThreshold;
	}
	
	// Rush: 중거리 이상
	if (PatternId == FName("Rush"))
	{
		return Distance > CloseRangeThreshold;
	}
	
	// 알 수 없는 패턴은 항상 허용
	return true;
}

FName UCBossPatternManager::GetFallbackPattern(FName OriginalPattern, float Distance) const
{
	// 근거리
	if (Distance <= CloseRangeThreshold)
	{
		// BasicAttack이 기본
		if (OriginalPattern == FName("Slam"))
		{
			return FName("Slam");
		}
		return FName("BasicAttack");
	}
	
	// 중거리
	if (Distance <= MidRangeThreshold)
	{
		// Barrage가 기본
		if (OriginalPattern == FName("Rush"))
		{
			return FName("Rush");
		}
		return FName("Barrage");
	}
	
	// 원거리
	return FName("Rush");
}

UCBossPatternBase* UCBossPatternManager::FindPattern(FName PatternId) const
{
	if (const TObjectPtr<UCBossPatternBase>* Found = PatternMap.Find(PatternId))
	{
		return *Found;
	}
	return nullptr;
}

void UCBossPatternManager::CleanupAllPatterns()
{
	for (const auto& Pair : PatternMap)
	{
		if (UCBossPatternBase* Pattern = Pair.Value)
		{
			Pattern->Cleanup();
		}
	}
	CurrentPatternId = NAME_None;
	bIsPatternActive = false;
}

void UCBossPatternManager::NotifyCurrentPatternEnd(bool bSuccess)
{
	if (!PhaseComponent) return;
	PhaseComponent->NotifyPatternFinished(bSuccess);
}

// ========================================
// Phase Transition
// ========================================

void UCBossPatternManager::PlayPhaseTransition(int32 PhaseIndex)
{
	if (!OwnerBoss) return;
	
	const FVector BossLocation = OwnerBoss->GetActorLocation();
	
	if (PhaseChangeEffect)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), PhaseChangeEffect, BossLocation);
	}
	
	if (PhaseChangeSound)
	{
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), PhaseChangeSound, BossLocation);
	}
}

void UCBossPatternManager::UpdatePhaseStats(int32 PhaseIndex)
{
	if (!OwnerBoss || !OwnerBoss->GetCharacterMovement()) return;
	
	if (PhaseWalkSpeeds.IsValidIndex(PhaseIndex))
	{
		OwnerBoss->GetCharacterMovement()->MaxWalkSpeed = PhaseWalkSpeeds[PhaseIndex];
	}
}

// ========================================
// Spawn System
// ========================================

void UCBossPatternManager::SpawnPatternActors(const FBossPatternDefinition& PatternData)
{
	SpawnPatternWeapons(PatternData);
	SpawnPatternUtilities(PatternData);
	SpawnPatternMinions(PatternData);
	StartProjectileRain(PatternData.ProjectileRain);
}

void UCBossPatternManager::SpawnPatternWeapons(const FBossPatternDefinition& PatternData)
{
	if (!GetWorld()) return;
	
	for (const FBossPatternWeaponSpawnDefinition& Definition : PatternData.WeaponSpawns)
	{
		if (!Definition.WeaponClass) continue;
		
		const FTransform SpawnTransform = ResolveSpawnTransform(Definition.SpawnTransform);
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = OwnerBoss;
		SpawnParams.Instigator = OwnerBoss;
		
		AActor* SpawnedActor = GetWorld()->SpawnActor<AActor>(Definition.WeaponClass, SpawnTransform, SpawnParams);
		if (!SpawnedActor) continue;
		
		if (Definition.bAttachToSpawnAnchor)
		{
			switch (Definition.SpawnTransform.Anchor)
			{
			case EBossPatternSpawnAnchor::BossSocket:
				if (OwnerBoss && OwnerBoss->GetMesh())
				{
					SpawnedActor->AttachToComponent(OwnerBoss->GetMesh(), FAttachmentTransformRules::KeepWorldTransform, Definition.SpawnTransform.SocketName);
				}
				break;
			
			case EBossPatternSpawnAnchor::BossRoot:
				if (OwnerBoss)
				{
					SpawnedActor->AttachToActor(OwnerBoss, FAttachmentTransformRules::KeepWorldTransform);
				}
				break;
			
			case EBossPatternSpawnAnchor::CustomActor:
				if (AActor* AnchorActor = Definition.SpawnTransform.SpawnAnchor.Get())
				{
					if (USceneComponent* AnchorRoot = AnchorActor->GetRootComponent())
					{
						SpawnedActor->AttachToComponent(AnchorRoot, FAttachmentTransformRules::KeepWorldTransform);
					}
					else
					{
						SpawnedActor->AttachToActor(AnchorActor, FAttachmentTransformRules::KeepWorldTransform);
					}
				}
				break;
			}
		}
		
		RegisterSpawnedActor(SpawnedActor, Definition.bDestroyOnPatternEnd, ActiveWeaponActors);
		ApplyInitialVelocity(SpawnedActor, Definition.InitialVelocity);
	}
}

void UCBossPatternManager::SpawnPatternUtilities(const FBossPatternDefinition& PatternData)
{
	if (!GetWorld()) return;
	
	for (const FBossPatternUtilitySpawnDefinition& Definition : PatternData.UtilitySpawns)
	{
		if (!Definition.ActorClass) continue;
		
		const int32 DesiredCount = FMath::Max(Definition.SpawnCount, 1);
		
		if (Definition.SpawnInterval > KINDA_SMALL_NUMBER && DesiredCount > 1)
		{
			FBossUtilitySpawnRuntime Runtime;
			Runtime.Definition = Definition;
			
			const int32 RuntimeId = ++UtilitySpawnRuntimeIdCounter;
			ActiveUtilitySpawnRuntimes.Add(RuntimeId, Runtime);
			
			HandleUtilitySpawnTimer(RuntimeId);
			
			if (UWorld* World = GetWorld())
			{
				FTimerDelegate Delegate = FTimerDelegate::CreateUObject(this, &UCBossPatternManager::HandleUtilitySpawnTimer, RuntimeId);
				World->GetTimerManager().SetTimer(ActiveUtilitySpawnRuntimes[RuntimeId].TimerHandle, Delegate, Definition.SpawnInterval, true);
			}
		}
		else
		{
			for (int32 Index = 0; Index < DesiredCount; ++Index)
			{
				SpawnUtilityActorImmediate(Definition);
			}
		}
	}
}

void UCBossPatternManager::SpawnPatternMinions(const FBossPatternDefinition& PatternData)
{
	if (!GetWorld()) return;
	
	for (const FBossPatternMinionSpawnDefinition& Definition : PatternData.MinionSpawns)
	{
		if (!Definition.MinionClass) continue;
		
		if (Definition.SpawnDelay > KINDA_SMALL_NUMBER)
		{
			FBossMinionSpawnRuntime Runtime;
			Runtime.Definition = Definition;
			const int32 RuntimeId = ++MinionSpawnRuntimeIdCounter;
			ActiveMinionSpawnRuntimes.Add(RuntimeId, Runtime);
			
			if (UWorld* World = GetWorld())
			{
				FTimerDelegate Delegate = FTimerDelegate::CreateUObject(this, &UCBossPatternManager::HandleMinionSpawnTimer, RuntimeId);
				World->GetTimerManager().SetTimer(ActiveMinionSpawnRuntimes[RuntimeId].TimerHandle, Delegate, Definition.SpawnDelay, false);
			}
		}
		else
		{
			SpawnMinionBatch(Definition);
		}
	}
}

void UCBossPatternManager::CleanupPatternActors()
{
	StopProjectileRain(false);
	
	auto CleanupActorArray = [](TArray<FBossSpawnedActorEntry>& Entries)
	{
		for (FBossSpawnedActorEntry& Entry : Entries)
		{
			if (Entry.bDestroyOnPatternEnd && Entry.Actor.IsValid())
			{
				Entry.Actor->Destroy();
			}
		}
		Entries.Reset();
	};
	
	CleanupActorArray(ActiveWeaponActors);
	CleanupActorArray(ActiveUtilityActors);
	
	for (FBossSpawnedMinionEntry& Entry : ActiveMinions)
	{
		if (Entry.bDestroyOnPatternEnd && Entry.Pawn.IsValid())
		{
			Entry.Pawn->Destroy();
		}
	}
	ActiveMinions.Reset();
}

void UCBossPatternManager::CleanupUtilitySpawnTimers()
{
	if (UWorld* World = GetWorld())
	{
		for (auto& Pair : ActiveUtilitySpawnRuntimes)
		{
			World->GetTimerManager().ClearTimer(Pair.Value.TimerHandle);
		}
	}
	ActiveUtilitySpawnRuntimes.Empty();
}

void UCBossPatternManager::CleanupMinionSpawnTimers()
{
	if (UWorld* World = GetWorld())
	{
		for (auto& Pair : ActiveMinionSpawnRuntimes)
		{
			World->GetTimerManager().ClearTimer(Pair.Value.TimerHandle);
		}
	}
	ActiveMinionSpawnRuntimes.Empty();
}

FTransform UCBossPatternManager::ResolveSpawnTransform(const FBossPatternSpawnTransform& SpawnTransform) const
{
	FVector BaseLocation = FVector::ZeroVector;
	FRotator BaseRotation = FRotator::ZeroRotator;
	
	switch (SpawnTransform.Anchor)
	{
	case EBossPatternSpawnAnchor::BossRoot:
		if (OwnerBoss)
		{
			BaseLocation = OwnerBoss->GetActorLocation();
			BaseRotation = OwnerBoss->GetActorRotation();
		}
		break;
		
	case EBossPatternSpawnAnchor::BossSocket:
		if (OwnerBoss && OwnerBoss->GetMesh())
		{
			if (SpawnTransform.SocketName != NAME_None && OwnerBoss->GetMesh()->DoesSocketExist(SpawnTransform.SocketName))
			{
				const FTransform SocketTransform = OwnerBoss->GetMesh()->GetSocketTransform(SpawnTransform.SocketName);
				BaseLocation = SocketTransform.GetLocation();
				BaseRotation = SocketTransform.Rotator();
			}
			else
			{
				BaseLocation = OwnerBoss->GetActorLocation();
				BaseRotation = OwnerBoss->GetActorRotation();
			}
		}
		break;
		
	case EBossPatternSpawnAnchor::PlayerLocation:
		if (AActor* Target = GetPlayerTarget())
		{
			BaseLocation = Target->GetActorLocation();
			BaseRotation = Target->GetActorRotation();
		}
		break;
		
	case EBossPatternSpawnAnchor::CustomActor:
		if (!SpawnTransform.SpawnAnchor.IsNull())
		{
			AActor* AnchorActor = SpawnTransform.SpawnAnchor.Get();
			if (!AnchorActor)
			{
				AnchorActor = SpawnTransform.SpawnAnchor.LoadSynchronous();
			}
			
			if (AnchorActor)
			{
				BaseLocation = AnchorActor->GetActorLocation();
				BaseRotation = AnchorActor->GetActorRotation();
			}
		}
		break;
	}
	
	if (!SpawnTransform.bUseAnchorRotation)
	{
		BaseRotation = FRotator::ZeroRotator;
	}
	
	const FQuat BaseQuat = BaseRotation.Quaternion();
	FVector Location = BaseLocation + (SpawnTransform.bUseAnchorRotation ? BaseQuat.RotateVector(SpawnTransform.LocationOffset) : SpawnTransform.LocationOffset);
	FRotator Rotation = SpawnTransform.bUseAnchorRotation ? (BaseRotation + SpawnTransform.RotationOffset) : SpawnTransform.RotationOffset;
	
	if (SpawnTransform.bProjectToGround && GetWorld())
	{
		FHitResult HitResult;
		const FVector Start = Location;
		const FVector End = Start - FVector::UpVector * SpawnTransform.GroundTraceDistance;
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BossPatternGroundTrace), false, OwnerBoss);
		
		if (!SpawnTransform.SpawnAnchor.IsNull())
		{
			if (AActor* AnchorActor = SpawnTransform.SpawnAnchor.Get())
			{
				QueryParams.AddIgnoredActor(AnchorActor);
			}
		}
		
		if (GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, SpawnTransform.GroundTraceChannel, QueryParams))
		{
			Location = HitResult.Location;
			if (SpawnTransform.bAlignToGroundNormal)
			{
				Rotation = HitResult.Normal.Rotation();
			}
		}
	}
	
	return FTransform(Rotation, Location);
}

void UCBossPatternManager::RegisterSpawnedActor(AActor* Actor, bool bDestroyOnPatternEnd, TArray<FBossSpawnedActorEntry>& Container)
{
	if (!Actor) return;
	
	FBossSpawnedActorEntry Entry;
	Entry.Actor = Actor;
	Entry.bDestroyOnPatternEnd = bDestroyOnPatternEnd;
	Container.Add(Entry);
}

void UCBossPatternManager::RegisterSpawnedMinion(APawn* Pawn, bool bDestroyOnPatternEnd)
{
	if (!Pawn) return;
	
	FBossSpawnedMinionEntry Entry;
	Entry.Pawn = Pawn;
	Entry.bDestroyOnPatternEnd = bDestroyOnPatternEnd;
	ActiveMinions.Add(Entry);
}

void UCBossPatternManager::ApplyInitialVelocity(AActor* SpawnedActor, const FVector& InitialVelocity) const
{
	if (!SpawnedActor || InitialVelocity.IsNearlyZero()) return;
	
	if (UProjectileMovementComponent* ProjectileMovement = SpawnedActor->FindComponentByClass<UProjectileMovementComponent>())
	{
		ProjectileMovement->Velocity = InitialVelocity;
		return;
	}
	
	if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(SpawnedActor->GetRootComponent()))
	{
		if (PrimitiveComponent->IsSimulatingPhysics())
		{
			PrimitiveComponent->SetPhysicsLinearVelocity(InitialVelocity);
		}
	}
}

void UCBossPatternManager::SpawnUtilityActorImmediate(const FBossPatternUtilitySpawnDefinition& Definition)
{
	if (!GetWorld() || !Definition.ActorClass) return;
	
	FTransform SpawnTransform = ResolveSpawnTransform(Definition.SpawnTransform);
	
	if (Definition.SpawnRadius > 0.f)
	{
		const FVector2D RandomOffset = FMath::RandPointInCircle(Definition.SpawnRadius);
		SpawnTransform.AddToTranslation(FVector(RandomOffset.X, RandomOffset.Y, 0.f));
	}
	
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OwnerBoss;
	SpawnParams.Instigator = OwnerBoss;
	
	if (AActor* SpawnedActor = GetWorld()->SpawnActor<AActor>(Definition.ActorClass, SpawnTransform, SpawnParams))
	{
		RegisterSpawnedActor(SpawnedActor, Definition.bDestroyOnPatternEnd, ActiveUtilityActors);
	}
}

void UCBossPatternManager::SpawnMinionBatch(const FBossPatternMinionSpawnDefinition& Definition)
{
	if (!GetWorld() || !Definition.MinionClass) return;
	
	const int32 DesiredCount = FMath::Max(Definition.SpawnCount, 1);
	
	for (int32 Index = 0; Index < DesiredCount; ++Index)
	{
		FTransform SpawnTransform = ResolveSpawnTransform(Definition.SpawnTransform);
		
		if (Definition.SpawnRadius > 0.f)
		{
			const FVector2D RandomOffset = FMath::RandPointInCircle(Definition.SpawnRadius);
			SpawnTransform.AddToTranslation(FVector(RandomOffset.X, RandomOffset.Y, 0.f));
		}
		
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = OwnerBoss;
		SpawnParams.Instigator = OwnerBoss;
		
		if (APawn* SpawnedPawn = GetWorld()->SpawnActor<APawn>(Definition.MinionClass, SpawnTransform, SpawnParams))
		{
			if (Definition.bSpawnDefaultController && !SpawnedPawn->GetController())
			{
				SpawnedPawn->SpawnDefaultController();
			}
			RegisterSpawnedMinion(SpawnedPawn, Definition.bDestroyOnPatternEnd);
		}
	}
}

void UCBossPatternManager::HandleUtilitySpawnTimer(int32 RuntimeId)
{
	FBossUtilitySpawnRuntime* Runtime = ActiveUtilitySpawnRuntimes.Find(RuntimeId);
	if (!Runtime) return;
	
	SpawnUtilityActorImmediate(Runtime->Definition);
	Runtime->SpawnedCount++;
	
	const int32 DesiredCount = FMath::Max(Runtime->Definition.SpawnCount, 1);
	if (Runtime->SpawnedCount >= DesiredCount)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(Runtime->TimerHandle);
		}
		ActiveUtilitySpawnRuntimes.Remove(RuntimeId);
	}
}

void UCBossPatternManager::HandleMinionSpawnTimer(int32 RuntimeId)
{
	FBossMinionSpawnRuntime* Runtime = ActiveMinionSpawnRuntimes.Find(RuntimeId);
	if (!Runtime) return;
	
	SpawnMinionBatch(Runtime->Definition);
	
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(Runtime->TimerHandle);
	}
	ActiveMinionSpawnRuntimes.Remove(RuntimeId);
}

void UCBossPatternManager::StartProjectileRain(const FBossPatternProjectileRainSettings& RainSettings)
{
	if (!RainSettings.bEnableRain || !RainSettings.ProjectileClass) return;
	if (!GetWorld()) return;
	
	StopProjectileRain(false);
	
	ActiveProjectileRainSettings = RainSettings;
	bProjectileRainActive = true;
	ProjectileRainWaveCounter = 0;
	
	HandleProjectileRainTick();
	if (!bProjectileRainActive) return;
	
	const float Interval = FMath::Max(RainSettings.SpawnInterval, 0.01f);
	
	if (RainSettings.Waves == 0 || ProjectileRainWaveCounter < RainSettings.Waves)
	{
		FTimerDelegate Delegate = FTimerDelegate::CreateUObject(this, &UCBossPatternManager::HandleProjectileRainTick);
		GetWorld()->GetTimerManager().SetTimer(ProjectileRainTimerHandle, Delegate, Interval, true);
	}
}

void UCBossPatternManager::HandleProjectileRainTick()
{
	if (!bProjectileRainActive || !GetWorld() || !ActiveProjectileRainSettings.ProjectileClass)
	{
		StopProjectileRain(false);
		return;
	}
	
	if (ActiveProjectileRainSettings.Waves > 0 && ProjectileRainWaveCounter >= ActiveProjectileRainSettings.Waves)
	{
		StopProjectileRain(false);
		return;
	}
	
	SpawnProjectileRainWave();
	++ProjectileRainWaveCounter;
	
	if (ActiveProjectileRainSettings.Waves > 0 && ProjectileRainWaveCounter >= ActiveProjectileRainSettings.Waves)
	{
		StopProjectileRain(false);
	}
}

void UCBossPatternManager::StopProjectileRain(bool bNotifyPatternEnd)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ProjectileRainTimerHandle);
	}
	
	bProjectileRainActive = false;
	ProjectileRainWaveCounter = 0;
	ActiveProjectileRainSettings = FBossPatternProjectileRainSettings();
}

void UCBossPatternManager::SpawnProjectileRainWave()
{
	if (!GetWorld() || !ActiveProjectileRainSettings.ProjectileClass) return;
	
	FTransform CenterTransform = ResolveSpawnTransform(ActiveProjectileRainSettings.SpawnTransform);
	CenterTransform.AddToTranslation(FVector(0.f, 0.f, ActiveProjectileRainSettings.SpawnHeight));
	
	for (int32 Index = 0; Index < FMath::Max(ActiveProjectileRainSettings.ProjectilesPerWave, 1); ++Index)
	{
		FTransform SpawnTransform = CenterTransform;
		
		if (ActiveProjectileRainSettings.SpawnRadius > 0.f)
		{
			const FVector2D RandomOffset = FMath::RandPointInCircle(ActiveProjectileRainSettings.SpawnRadius);
			SpawnTransform.AddToTranslation(FVector(RandomOffset.X, RandomOffset.Y, 0.f));
		}
		
		FRotator SpawnRotation = ActiveProjectileRainSettings.InitialVelocity.IsNearlyZero()
			? FRotator(-90.f, 0.f, 0.f)
			: ActiveProjectileRainSettings.InitialVelocity.GetSafeNormal().Rotation();
		
		SpawnTransform.SetRotation(SpawnRotation.Quaternion());
		
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = OwnerBoss;
		SpawnParams.Instigator = OwnerBoss;
		
		if (AActor* Projectile = GetWorld()->SpawnActor<AActor>(ActiveProjectileRainSettings.ProjectileClass, SpawnTransform, SpawnParams))
		{
			RegisterSpawnedActor(Projectile, ActiveProjectileRainSettings.bDestroyOnPatternEnd, ActiveUtilityActors);
			ApplyInitialVelocity(Projectile, ActiveProjectileRainSettings.InitialVelocity);
		}
	}
}