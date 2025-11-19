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
		World->GetTimerManager().ClearTimer(CooldownTimerHandle);
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
// Core Logic: Pattern Execution & Lifecycle
// ========================================

void UCBossPatternManager::HandlePatternStarted(int32 PhaseIndex, FName PatternId, const FBossPatternDefinition& PatternData, float RemainingPower)
{
	if (!PhaseComponent || !PhaseComponent->IsBattleStarted()) return;

	// 기존 패턴 ID 저장 (Fallback 감지)
	FName OriginalPatternId = PatternId;
	FBossPatternDefinition FinalPatternData = PatternData;

	float DistanceToPlayer = GetDistanceToPlayer();
	if (!ValidatePatternDistance(PatternId, DistanceToPlayer))
	{
		FName FallbackPatternId = GetFallbackPattern(PatternId, DistanceToPlayer);
		UE_LOG(LogTemp, Warning, TEXT("[PatternManager] Pattern %s invalid at distance %.1f, using fallback: %s"), 
			*PatternId.ToString(), DistanceToPlayer, *FallbackPatternId.ToString());
		
		// Fallback 패턴도 유효한지 검증
		if (!ValidatePatternDistance(FallbackPatternId, DistanceToPlayer))
		{
			// Fallback도 실패하면 BasicAttack으로 강제 변경
			UE_LOG(LogTemp, Error, TEXT("[PatternManager] Fallback pattern %s also invalid! Forcing BasicAttack"), 
				*FallbackPatternId.ToString());
			FallbackPatternId = FName("BasicAttack");
		}
		
		PatternId = FallbackPatternId;
	}

	// Fallback이 발생했다면 맞는 PatternData 찾기
	if (PatternId != OriginalPatternId)
	{
		bool bFoundNewPatternData = false;
		
		// PhaseComponent에서 현재 Phase의 패턴 목록 가져오기
		const FBossPhaseDefinition* CurrentPhase = PhaseComponent->GetCurrentPhaseDefinition();
		if (CurrentPhase)
		{
			for (const FBossPatternDefinition& Pattern : CurrentPhase->Patterns)
			{
				if (Pattern.PatternId == PatternId)
				{
					FinalPatternData = Pattern;
					bFoundNewPatternData = true;
					UE_LOG(LogTemp, Log, TEXT("[PatternManager]  Found Fallback PatternData: %s (Exec: %.2f, Recovery: %.2f, Cooldown: %.2f)"), 
						*PatternId.ToString(), Pattern.ExecutionTime, Pattern.RecoveryTime, Pattern.Cooldown);
					break;
				}
			}
		}
		
		if (!bFoundNewPatternData)
		{
			UE_LOG(LogTemp, Error, TEXT("[PatternManager]  Could not find PatternData for fallback pattern %s! Using original data."), 
				*PatternId.ToString());
		}
	}

	UCBossPatternBase* SelectedPattern = FindPattern(PatternId);
	if (!SelectedPattern)
	{
		UE_LOG(LogTemp, Error, TEXT("[PatternManager] Pattern object not found for ID: %s. Trying BasicAttack..."), 
			*PatternId.ToString());
		
		SelectedPattern = FindPattern(FName("BasicAttack"));
		if (!SelectedPattern)
		{
			UE_LOG(LogTemp, Error, TEXT("[PatternManager] BasicAttack also not found! Skipping pattern execution."));
			
			// 패턴 실행을 건너뛰고 즉시 다음 패턴 선택 준비
			State = EBossManagerState::Idle;
			bIsPatternActive = false;
			SelectNextPattern();
			return;
		}
		
		// BasicAttack으로 Fallback된 경우에도 올바른 PatternData 찾기
		PatternId = FName("BasicAttack");
		if (PatternId != OriginalPatternId)
		{
			const FBossPhaseDefinition* CurrentPhase = PhaseComponent->GetCurrentPhaseDefinition();
			if (CurrentPhase)
			{
				for (const FBossPatternDefinition& Pattern : CurrentPhase->Patterns)
				{
					if (Pattern.PatternId == PatternId)
					{
						FinalPatternData = Pattern;
						UE_LOG(LogTemp, Log, TEXT("[PatternManager]  Found BasicAttack PatternData (Exec: %.2f, Recovery: %.2f)"), 
							Pattern.ExecutionTime, Pattern.RecoveryTime);
						break;
					}
				}
			}
		}
	}

	CurrentPatternId = PatternId;
	bIsPatternActive = true; // PhaseComponent가 대기하도록 설정
	State = EBossManagerState::Executing;

	CleanupPatternActors();
	CleanupUtilitySpawnTimers();
	CleanupMinionSpawnTimers();
	StopProjectileRain(false);

	SpawnPatternActors(FinalPatternData);

	CurrentPattern = SelectedPattern;
	CurrentPattern->ExecutePattern(PhaseIndex, FinalPatternData); 
	
	UE_LOG(LogTemp, Warning, TEXT("[PatternManager] Pattern %s execution started with ExecutionTime: %.2f, RecoveryTime: %.2f"), 
		*PatternId.ToString(), FinalPatternData.ExecutionTime, FinalPatternData.RecoveryTime);
}

void UCBossPatternManager::NotifyCurrentPatternEnd(bool bApplyCooldown)
{
	UE_LOG(LogTemp, Warning, TEXT("[PatternManager] NotifyCurrentPatternEnd Called. ApplyCooldown: %s"), 
		bApplyCooldown ? TEXT("YES") : TEXT("NO"));

	State = EBossManagerState::Cooldown;

	if (bApplyCooldown && CurrentPattern)
	{
		float WaitTime = CurrentPattern->GetRuntimeCooldown();
		
		WaitTime = FMath::Max(WaitTime, MinGlobalCooldown);

		UE_LOG(LogTemp, Log, TEXT("[PatternManager] Cooling down for %.2f sec"), WaitTime);

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				CooldownTimerHandle, 
				this, 
				&UCBossPatternManager::OnCooldownFinished, 
				WaitTime, 
				false
			);
		}
	}
	else
	{
		// Forced Stop 여부 확인
		bool bForcedStop = !bApplyCooldown;
		float CooldownDuration = bForcedStop ? (MinGlobalCooldown * 2.0f) : 0.0f;
		
		if (CooldownDuration > 0.0f)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PatternManager] Forced Stop - Applying extended min cooldown: %.2f"), CooldownDuration);
			
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimer(
					CooldownTimerHandle, 
					this, 
					&UCBossPatternManager::OnCooldownFinished, 
					CooldownDuration, 
					false
				);
			}
		}
		else
		{
			// 쿨다운 없으면 즉시 종료 처리
			OnCooldownFinished();
		}
	}

	CurrentPattern = nullptr;
}

void UCBossPatternManager::OnCooldownFinished()
{
	UE_LOG(LogTemp, Log, TEXT("[PatternManager] Cooldown Finished. Ready for next pattern."));

	State = EBossManagerState::Idle;
	
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(CooldownTimerHandle);
	}
	
	// 스폰된 액터들 정리
	CleanupPatternActors();
	CleanupUtilitySpawnTimers();
	CleanupMinionSpawnTimers();

	if (PhaseComponent)
	{
		UE_LOG(LogTemp, Log, TEXT("[PatternManager] Notifying PhaseComponent of pattern completion"));
		PhaseComponent->FinishPattern(false);  // false = 정상 종료
	}
	
	bIsPatternActive = false;
	
	UE_LOG(LogTemp, Log, TEXT("[PatternManager] Pattern completed, manager ready for next pattern"));
	
}

void UCBossPatternManager::SelectNextPattern()
{
	if (!PhaseComponent || !PhaseComponent->IsBattleStarted())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PatternManager] SelectNextPattern: PhaseComponent not ready"));
		return;
	}

	State = EBossManagerState::Idle;
	bIsPatternActive = false;
	CurrentPatternId = NAME_None;
	CurrentPattern = nullptr;

	UE_LOG(LogTemp, Log, TEXT("[PatternManager] Ready for next pattern selection from PhaseComponent"));

	EBossBattleState PhaseState = PhaseComponent->GetCurrentState();
	
	switch (PhaseState)
	{
		case EBossBattleState::ExecutingPattern:
			UE_LOG(LogTemp, Log, TEXT("[PatternManager] PhaseComponent is still executing pattern, waiting..."));
			break;
			
		case EBossBattleState::Recover:
			UE_LOG(LogTemp, Log, TEXT("[PatternManager] PhaseComponent is in recovery, will auto-select next pattern"));
			break;
			
		case EBossBattleState::Shout:
			UE_LOG(LogTemp, Log, TEXT("[PatternManager] PhaseComponent is shouting"));
			break;
			
		case EBossBattleState::Dead:
			UE_LOG(LogTemp, Warning, TEXT("[PatternManager] Boss is dead, no next pattern"));
			break;
			
		default:
			UE_LOG(LogTemp, Log, TEXT("[PatternManager] PhaseComponent state: %d, ready for pattern selection"), (int32)PhaseState);
			break;
	}
}

// PhaseComponent에서 직접 호출되는 종료 델리게이트
void UCBossPatternManager::HandlePatternFinished(int32 PhaseIndex, FName PatternId, const FBossPatternDefinition& PatternData, float RemainingPower)
{
	// 재진입 방지 - 근데 왜 작동을 안하니 아....
	if (State == EBossManagerState::Cooldown)
	{
		UE_LOG(LogTemp, Log, TEXT("[PatternManager] HandlePatternFinished ignored - already in Cooldown"));
		return;
	}

	// PhaseComponent 등 외부에서 강제로 종료 신호를 보낸 경우
	UE_LOG(LogTemp, Warning, TEXT("[PatternManager] HandlePatternFinished (Forced Stop)"));
	
	if (CurrentPattern)
	{
		CurrentPattern->OnPatternEnd();
		CurrentPattern = nullptr;
	}


	if (PhaseComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PatternManager] Notifying PhaseComponent of forced pattern end"));
		PhaseComponent->FinishPattern(true); // bInterrupted=true, power loss + cooldown
	}
	State = EBossManagerState::Cooldown;
	
	UE_LOG(LogTemp, Log, TEXT("[PatternManager] Applying minimum cooldown: %.2f sec"), MinGlobalCooldown);
	
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			CooldownTimerHandle, 
			this, 
			&UCBossPatternManager::OnCooldownFinished, 
			MinGlobalCooldown, 
			false
		);
	}
	
	// 즉시 정리
	CleanupUtilitySpawnTimers();
	CleanupMinionSpawnTimers();
	CleanupPatternActors();
}


// ========================================
// Delegate Binding & Event Handlers
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

void UCBossPatternManager::HandleShoutStarted(int32 PhaseIndex, FName ShoutId, float Duration)
{
	// 포효 패턴 처리 로직 (필요시 구현)
}

// ========================================
// Rush Pattern Helpers
// ========================================

void UCBossPatternManager::HandleRushMovementStart()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(RushTimerHandle);
	}
	
	if (UCBossPattern_Rush* LocalRushPattern = Cast<UCBossPattern_Rush>(FindPattern(FName("Rush"))))
	{
		if(!LocalRushPattern->IsRushing()) 
			LocalRushPattern->Anim_RushStart(); 
	}
}

void UCBossPatternManager::HandleRushMovementStop()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(RushTimerHandle);
	}
	
	if (UCBossPattern_Rush* LocalRushPattern = Cast<UCBossPattern_Rush>(FindPattern(FName("Rush"))))
	{
		ERushState RushState = LocalRushPattern->GetRushState();
		
		// Recovery나 Cooldown 상태면 이미 정상 종료 중이므로 무시
		if (RushState == ERushState::Recovery || RushState == ERushState::Cooldown || RushState == ERushState::Idle)
		{
			UE_LOG(LogTemp, Log, TEXT("[PatternManager] HandleRushMovementStop ignored - Rush already ending (State: %d)"), (int32)RushState);
			return;
		}
		
		// Telegraph나 Rushing 상태일 때만 강제 종료
		UE_LOG(LogTemp, Warning, TEXT("[PatternManager] HandleRushMovementStop - Forcing Rush stop (State: %d)"), (int32)RushState);
		LocalRushPattern->HandleRushMovementStop();
	}
}

// ========================================
// Utilities Implementation
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
	if (PatternId == FName("BasicAttack") || PatternId == FName("Slam"))
	{
		return Distance <= CloseRangeThreshold;
	}
	if (PatternId == FName("Barrage"))
	{
		return Distance > CloseRangeThreshold && Distance <= MidRangeThreshold;
	}
	if (PatternId == FName("Rush"))
	{
		return Distance > CloseRangeThreshold;
	}
	return true;
}

FName UCBossPatternManager::GetFallbackPattern(FName OriginalPattern, float Distance) const
{
	if (Distance <= CloseRangeThreshold)
	{
		return (OriginalPattern == FName("Slam")) ? FName("Slam") : FName("BasicAttack");
	}
	if (Distance <= MidRangeThreshold)
	{
		return (OriginalPattern == FName("Rush")) ? FName("Rush") : FName("Barrage");
	}
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
	CurrentPattern = nullptr;
}

// ========================================
// Phase Transition & Stats
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
// Spawn System Implementation 
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