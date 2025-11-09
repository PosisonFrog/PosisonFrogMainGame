#include "CBossPatternManager.h"

#include "00_Character/01_Enemy/CEnemyBossCharacter.h"
#include "00_Character/02_Component/01_EnemyComponent/CEnemyBossPhaseComponent.h"
#include "00_Character/02_Component/01_EnemyComponent/CEnemyWeaponComponent.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "00_Character/01_Enemy/01_AIController/BossAIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "Camera/CameraShakeBase.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"

UCBossPatternManager::UCBossPatternManager()
{
	PrimaryComponentTick.bCanEverTick = true;  // Tick 활성화 (Rush 이동에 필요)
	bIsPatternActive = false;
	bShouldNotifyOnMontageEnd = false;
	bIsRushing = false;
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
	
	PhaseComponent = OwnerBoss->GetBossPhaseComponent();
	WeaponComponent = OwnerBoss->FindComponentByClass<UCEnemyWeaponComponent>();

	if (!PhaseComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("[PatternManager] BossPhaseComponent not found!"));
		return;
	}

	if (!WeaponComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("[PatternManager] WeaponComponent not found!"));
	}


	BindToBossPhaseComponent();
	// 몽타주 완료 델리게이트 초기화
	MontageEndDelegate.BindUObject(this, &UCBossPatternManager::OnMontageCompleted);
}

void UCBossPatternManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromBossPhaseComponent();

	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(RushDelayTimer);
		TimerManager.ClearTimer(RushMoveTimer);
		TimerManager.ClearTimer(BarrageLoopTimer);
		TimerManager.ClearTimer(BarrageStopTimer);
		TimerManager.ClearTimer(GroundCheckTimer);
		TimerManager.ClearTimer(PhaseTransitionTimer);
	}
	CleanupUtilitySpawnTimers();
	CleanupMinionSpawnTimers();
	CleanupPatternActors();
	StopProjectileRain(false);
	
	Super::EndPlay(EndPlayReason);
}

void UCBossPatternManager::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bIsRushing && OwnerBoss)
	{
		OwnerBoss->AddMovementInput(OwnerBoss->GetActorForwardVector(), 1.0f); // RushSpeed는 CharacterMovement에서 제어하도록 1.0f로 설정하거나, 여기서 직접 속도를 곱해줄 수 있습니다.
	}
}


// ========================================
// 델리게이트 바인딩
// ========================================

void UCBossPatternManager::BindToBossPhaseComponent()
{
	if (!PhaseComponent)
	{
		return;
	}

	PhaseComponent->OnPhaseChanged.AddDynamic(this, &UCBossPatternManager::HandlePhaseChanged);
	PhaseComponent->OnPatternStarted.AddDynamic(this, &UCBossPatternManager::HandlePatternStarted);
	PhaseComponent->OnPatternFinished.AddDynamic(this, &UCBossPatternManager::HandlePatternFinished);
	PhaseComponent->OnShoutStarted.AddDynamic(this, &UCBossPatternManager::HandleShoutStarted);

	UE_LOG(LogTemp, Log, TEXT("[PatternManager] Successfully bound to BossPhaseComponent"));
}

void UCBossPatternManager::UnbindFromBossPhaseComponent()
{
	if (!PhaseComponent)
	{
		return;
	}

	PhaseComponent->OnPhaseChanged.RemoveDynamic(this, &UCBossPatternManager::HandlePhaseChanged);
	PhaseComponent->OnPatternStarted.RemoveDynamic(this, &UCBossPatternManager::HandlePatternStarted);
	PhaseComponent->OnPatternFinished.RemoveDynamic(this, &UCBossPatternManager::HandlePatternFinished);
	PhaseComponent->OnShoutStarted.RemoveDynamic(this, &UCBossPatternManager::HandleShoutStarted);
}

// ========================================
// 이벤트 핸들러
// ========================================

void UCBossPatternManager::HandlePhaseChanged(int32 PhaseIndex, const FBossPhaseDefinition& PhaseData)
{
	UE_LOG(LogTemp, Warning, TEXT("[PatternManager] ===== PHASE %d: %s ====="), 
	       PhaseIndex + 1, *PhaseData.PhaseName.ToString());

	PlayPhaseTransition(PhaseIndex);
	UpdatePhaseStats(PhaseIndex);
}

void UCBossPatternManager::HandlePatternStarted(int32 PhaseIndex, FName PatternId, 
												const FBossPatternDefinition& PatternData, float RemainingPower)
{
	UE_LOG(LogTemp, Warning, TEXT("[PatternManager] Pattern Started: %s (Phase %d, Power: %.1f)"), 
		   *PatternId.ToString(), PhaseIndex, RemainingPower);

	// ===== 전투 시작 여부 체크 =====
	if (!PhaseComponent || !PhaseComponent->IsBattleStarted())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PatternManager] Pattern %s ignored - Battle not started"), 
			   *PatternId.ToString());
		return;
	}

	CurrentPatternId = PatternId;
	bIsPatternActive = true;
	bIsRushing = false;

	CleanupPatternActors();
	CleanupUtilitySpawnTimers();
	CleanupMinionSpawnTimers();
	StopProjectileRain(false);
	
	// ===== 특수 이동이 있는 패턴만 Chase 비활성화 =====
	if (PatternId == FName("Barrage"))
	{
		if (ABossAIController* BossAI = Cast<ABossAIController>(GetBossAI()))
		{
			BossAI->SetChaseEnabled(false);
			UE_LOG(LogTemp, Warning, TEXT("[PatternManager] Disabled chase for %s"), *PatternId.ToString());
		}
	}
	// RUsh, BasicAttack, Slam은 Chase 유지 → 계속 플레이어 추적
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[PatternManager] Chase maintained for %s"), *PatternId.ToString());
	}
	
	SpawnPatternActors(PatternData);
	
	// 패턴별 분기
	if (PatternId == FName("BasicAttack"))
	{
		ExecuteBasicAttack();
	}
	else if (PatternId == FName("Rush"))
	{
		ExecuteRushPattern();
	}
	else if (PatternId == FName("Slam"))
	{
		ExecuteSlamPattern();
	}
	else if (PatternId == FName("Barrage"))
	{
		ExecuteBarragePattern();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[PatternManager] Unknown pattern: %s, using basic attack"), 
			   *PatternId.ToString());
		ExecuteBasicAttack();
	}
}
void UCBossPatternManager::HandlePatternFinished(int32 PhaseIndex, FName PatternId, 
                                                 const FBossPatternDefinition& PatternData, float RemainingPower)
{
	UE_LOG(LogTemp, Log, TEXT("[PatternManager] Pattern Finished: %s"), *PatternId.ToString());

	bIsPatternActive = false;
	CleanupUtilitySpawnTimers();
		CleanupMinionSpawnTimers();
		CleanupPatternActors();
	if (PatternId == FName("Rush") || PatternId == FName("Barrage"))
	{
		if (ABossAIController* BossAI = Cast<ABossAIController>(GetBossAI()))
		{
			BossAI->SetChaseEnabled(true);
			UE_LOG(LogTemp, Warning, TEXT("[PatternManager] Re-enabled chase after %s"), 
				   *PatternId.ToString());
		}
	}

	// 패턴별 정리 작업
	if (PatternId == FName("Rush"))
	{
		bIsRushing = false;
		GetWorld()->GetTimerManager().ClearTimer(RushMoveTimer);
		
		// 속도 원복
		if (OwnerBoss && OwnerBoss->GetCharacterMovement() && PhaseComponent)
		{
			int32 CurrentPhase = PhaseComponent->GetCurrentPhaseIndex();
			if (PhaseWalkSpeeds.IsValidIndex(CurrentPhase))
			{
				OwnerBoss->GetCharacterMovement()->MaxWalkSpeed = PhaseWalkSpeeds[CurrentPhase];
			}
		}
	}
	else if (PatternId == FName("Barrage"))
	{
		StopBarrage();
	}
}

void UCBossPatternManager::HandleShoutStarted(int32 PhaseIndex, FName ShoutId, float Duration)
{
	UE_LOG(LogTemp, Warning, TEXT("[PatternManager] SHOUT: %s (Duration: %.2fs)"), 
	       *ShoutId.ToString(), Duration);

	// 샤우트 연출 (카메라 흔들림, 이펙트 등)
	if (GroundImpactShake)
	{
		UGameplayStatics::PlayWorldCameraShake(
			GetWorld(),
			GroundImpactShake,
			OwnerBoss->GetActorLocation(),
			0.f,
			5000.f
		);
	}

	// AOE 데미지
	TArray<FHitResult> HitResults;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(OwnerBoss);

	GetWorld()->SweepMultiByChannel(
		HitResults,
		OwnerBoss->GetActorLocation(),
		OwnerBoss->GetActorLocation(),
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeSphere(500.f),
		QueryParams
	);

	for (const FHitResult& Hit : HitResults)
	{
		if (AActor* HitActor = Hit.GetActor())
		{
			if (HitActor->ActorHasTag("Player"))
			{
				UGameplayStatics::ApplyDamage(
					HitActor,
					30.f,
					GetBossAI(),
					OwnerBoss,
					UDamageType::StaticClass()
				);
			}
		}
	}
}

// ========================================
// 패턴 실행: Basic Attack
// ========================================

void UCBossPatternManager::ExecuteBasicAttack()
{
	UE_LOG(LogTemp, Log, TEXT("[PatternManager] Executing Basic Attack"));

	if (!WeaponComponent)
	{
		PhaseComponent->NotifyPatternFinished(false);
		return;
	}

	int32 AttackIndex = DefaultAttackIndex;
	if (const int32* MappedIndex = PatternAttackIndexMap.Find(CurrentPatternId))
	{
		AttackIndex = *MappedIndex;
	}

	WeaponComponent->SetCurrentAttackIndex(AttackIndex);
	WeaponComponent->DoAttack();

	// 몽타주 완료 시 자동 알림
	PlayMontageAndNotify(nullptr, true);
}

// ========================================
// 패턴 실행: Rush
// ========================================

void UCBossPatternManager::ExecuteRushPattern()
{
	UE_LOG(LogTemp, Warning, TEXT("[PatternManager] ===== RUSH PATTERN START ====="));

	if (!OwnerBoss)
	{
		NotifyCurrentPatternEnd(false);
		return;
	}

	// 1. 타겟 위치 설정
	AActor* Target = GetPlayerTarget();
	if (!Target)
	{
		UE_LOG(LogTemp, Error, TEXT("[PatternManager] No target found for Rush!"));
		NotifyCurrentPatternEnd(false);
		return;
	}
	
	RushTargetLocation = Target->GetActorLocation();
	UE_LOG(LogTemp, Warning, TEXT("[PatternManager] Rush target: %s"), *RushTargetLocation.ToString());

	// 2. 이동 속도 설정
	if (OwnerBoss->GetCharacterMovement())
	{
		OwnerBoss->GetCharacterMovement()->MaxWalkSpeed = RushSpeed;
		UE_LOG(LogTemp, Log, TEXT("[PatternManager] Rush speed set to: %.1f"), RushSpeed);
	}
	
	UE_LOG(LogTemp, Log, TEXT("[PatternManager] Waiting for AnimNotify_StartRush..."));
}

void UCBossPatternManager::HandleRushMovementStart()
{
	if (!OwnerBoss || RushTargetLocation.IsZero())  // 수정: 타겟 위치가 0이면 return
	{
		UE_LOG(LogTemp, Error, TEXT("[PatternManager] Cannot start rush - invalid owner or target location"));
		return;
	}

	AAIController* AIController = GetBossAI();
	if (!AIController)
	{
		UE_LOG(LogTemp, Error, TEXT("[PatternManager] Cannot start rush - no AI controller"));
		return;
	}

	bIsRushing = true;
	if (OwnerBoss)
	{
		OwnerBoss->SetIsBossRushing(true);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[PatternManager] OwnerBoss is null in HandleRushMovementStart"));
	}
    
	UE_LOG(LogTemp, Warning, TEXT("[PatternManager] Starting MoveTo: %s"), *RushTargetLocation.ToString());
	
	// AI MoveTo 실행
	FAIMoveRequest MoveRequest(RushTargetLocation);
	MoveRequest.SetAcceptanceRadius(RushAcceptanceRadius);
	MoveRequest.SetUsePathfinding(false); // 직선 돌진
    
	FNavPathSharedPtr NavPath;
	EPathFollowingRequestResult::Type Result = AIController->MoveTo(MoveRequest, &NavPath);
	
	// 결과 로그
	switch(Result)
	{
		case EPathFollowingRequestResult::Failed:
			UE_LOG(LogTemp, Error, TEXT("[PatternManager] MoveTo FAILED!"));
			bIsRushing = false;
			if (OwnerBoss) {OwnerBoss->SetIsBossRushing(false);}
			break;
		case EPathFollowingRequestResult::AlreadyAtGoal:
			UE_LOG(LogTemp, Warning, TEXT("[PatternManager] Already at goal"));
			break;
		case EPathFollowingRequestResult::RequestSuccessful:
			UE_LOG(LogTemp, Log, TEXT("[PatternManager] MoveTo started successfully"));
			break;
	}
}

void UCBossPatternManager::HandleRushMovementStop()
{
	UE_LOG(LogTemp, Log, TEXT("[PatternManager] AnimNotify: StopRushMovement received!"));
	bIsRushing = false;
	if (OwnerBoss)
	{
		OwnerBoss->SetIsBossRushing(false);
	}

	// 관성을 제거하고 원래 속도로 복귀
	if (OwnerBoss && OwnerBoss->GetCharacterMovement())
	{
		OwnerBoss->GetCharacterMovement()->StopMovementImmediately();
		// 원래 걷기 속도로 되돌릴 수 있습니다. (예: PhaseWalkSpeeds 값 사용)
		if (PhaseComponent && PhaseWalkSpeeds.IsValidIndex(PhaseComponent->GetCurrentPhaseIndex()))
		{
			OwnerBoss->GetCharacterMovement()->MaxWalkSpeed = PhaseWalkSpeeds[PhaseComponent->GetCurrentPhaseIndex()];
		}
	}
}

/*
void UCBossPatternManager::DoRushMovement()
{
	AAIController* AI = GetBossAI();
	if (!AI)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PatternManager] No AI Controller for Rush!"));
		PhaseComponent->NotifyPatternFinished(false);
		return;
	}

	AActor* Target = GetPlayerTarget();
	if (!Target)
	{
		PhaseComponent->NotifyPatternFinished(false);
		return;
	}

	// MoveToActor 실행 (단순 버전)
	EPathFollowingRequestResult::Type MoveResult = AI->MoveToActor(
		Target,
		RushAcceptanceRadius,
		true,  // bStopOnOverlap
		true,  // bUsePathfinding
		true,  // bCanStrafe
		nullptr,  // FilterClass
		true  // bAllowPartialPath
	);

	if (MoveResult == EPathFollowingRequestResult::Failed)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PatternManager] Rush MoveTo failed!"));
		PhaseComponent->NotifyPatternFinished(false);
		return;
	}

	// 타임아웃 타이머 (3초 후 강제 완료)
	GetWorld()->GetTimerManager().SetTimer(
		RushMoveTimer,
		this,
		&UCBossPatternManager::OnRushCompleted,
		3.0f,
		false

		
	);
	

	// PathFollowingComponent에서 이동 완료 감지
	if (UPathFollowingComponent* PFC = AI->GetPathFollowingComponent())
	{
		FTimerHandle CheckTimer;
		GetWorld()->GetTimerManager().SetTimer(
			CheckTimer,
			[this, PFC]()
			{
				if (PFC->GetStatus() == EPathFollowingStatus::Idle ||
					PFC->GetStatus() == EPathFollowingStatus::Waiting)
				{
					OnRushCompleted();
				}
			},
			0.1f,
			true
		);
	}
}

void UCBossPatternManager::OnRushCompleted()
{
	UE_LOG(LogTemp, Log, TEXT("[PatternManager] Rush Completed"));
	GetWorld()->GetTimerManager().ClearTimer(RushMoveTimer);

	if (PhaseComponent)
	{
		PhaseComponent->NotifyPatternFinished(true);
	}
}*/

// ========================================
// 패턴 실행: Slam
// ========================================

void UCBossPatternManager::ExecuteSlamPattern()
{
	UE_LOG(LogTemp, Warning, TEXT("[PatternManager] SLAM ATTACK!"));

	if (!WeaponComponent)
	{
		PhaseComponent->NotifyPatternFinished(false);
		return;
	}

	if (const int32* AttackIndex = PatternAttackIndexMap.Find(FName("Slam")))
	{
		WeaponComponent->SetCurrentAttackIndex(*AttackIndex);
		WeaponComponent->DoAttack();
	}
	else
	{
		WeaponComponent->SetCurrentAttackIndex(DefaultAttackIndex);
		WeaponComponent->DoAttack();
	}

	// 몽타주 완료 시 자동 알림
	PlayMontageAndNotify(nullptr, true);
}

// ========================================
// 패턴 실행: Barrage
// ========================================

void UCBossPatternManager::ExecuteBarragePattern()
{
	UE_LOG(LogTemp, Warning, TEXT("[PatternManager]  BARRAGE ATTACK!"));

	if (!ProjectileClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[PatternManager] ProjectileClass not set!"));
		PhaseComponent->NotifyPatternFinished(false);
		return;
	}

	if (WeaponComponent)
	{
		const int32 BarrageAttackIndex = 3;  // Barrage 패턴용 몽타주 인덱스
    
		WeaponComponent->SetCurrentAttackIndex(BarrageAttackIndex);
    
		if (WeaponComponent->IsAttackIndexValid(BarrageAttackIndex))
		{
			WeaponComponent->DoAttack();
			UE_LOG(LogTemp, Log, TEXT("[PatternManager] Playing Barrage montage (Index: %d)"), BarrageAttackIndex);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[PatternManager] Barrage attack index %d is invalid!"), BarrageAttackIndex);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[PatternManager] WeaponComponent is missing for Barrage!"));
	}
	
	BarrageShotCount = 0;

	// 발사 루프 타이머
	GetWorld()->GetTimerManager().SetTimer(
		BarrageLoopTimer,
		this,
		&UCBossPatternManager::FireBarrageShot,
		BarrageShotInterval,
		true
	);

	// 총 지속 시간 후 중지
	GetWorld()->GetTimerManager().SetTimer(
		BarrageStopTimer,
		this,
		&UCBossPatternManager::StopBarrage,
		BarrageTotalDuration,
		false
	);
}

void UCBossPatternManager::FireBarrageShot()
{
	BarrageShotCount++;

	if (BarrageShotCount > MaxBarrageShots)
	{
		StopBarrage();
		return;
	}

	AActor* Target = GetPlayerTarget();
	if (!Target)
	{
		return;
	}

	// 발사 위치 계산
	FVector SpawnLocation = OwnerBoss->GetActorLocation() + 
	                        OwnerBoss->GetActorForwardVector() * 100.f +
	                        FVector(0, 0, 50.f);

	// 플레이어 방향으로 회전
	FRotator SpawnRotation = (Target->GetActorLocation() - SpawnLocation).Rotation();

	// 발사체 스폰
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OwnerBoss;
	SpawnParams.Instigator = OwnerBoss;

	AActor* Projectile = GetWorld()->SpawnActor<AActor>(
		ProjectileClass,
		SpawnLocation,
		SpawnRotation,
		SpawnParams
	);

	UE_LOG(LogTemp, Verbose, TEXT("[PatternManager] Fired projectile %d/%d"), 
	       BarrageShotCount, MaxBarrageShots);
}

void UCBossPatternManager::StopBarrage()
{
	GetWorld()->GetTimerManager().ClearTimer(BarrageLoopTimer);
	GetWorld()->GetTimerManager().ClearTimer(BarrageStopTimer);

	UE_LOG(LogTemp, Log, TEXT("[PatternManager] Barrage Stopped"));
	StopProjectileRain(false);
	
	if (PhaseComponent)
	{
		PhaseComponent->NotifyPatternFinished(true);
	}
}


// ========================================
// 페이즈 처리
// ========================================

void UCBossPatternManager::PlayPhaseTransition(int32 PhaseIndex)
{
	// 이펙트
	if (PhaseChangeEffect)
	{
		UGameplayStatics::SpawnEmitterAtLocation(
			GetWorld(),
			PhaseChangeEffect,
			OwnerBoss->GetActorLocation(),
			FRotator::ZeroRotator,
			FVector(3.f)
		);
	}

	// 사운드
	if (PhaseChangeSound)
	{
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), PhaseChangeSound, OwnerBoss->GetActorLocation());
	}

	// 카메라 흔들림 (페이즈별 강도)
	if (GroundImpactShake)
	{
		float ShakeScale = 1.f + (PhaseIndex * 0.5f); // 페이즈마다 강해짐
		UGameplayStatics::PlayWorldCameraShake(
			GetWorld(),
			GroundImpactShake,
			OwnerBoss->GetActorLocation(),
			0.f,
			2000.f,
			ShakeScale
		);
	}
}

void UCBossPatternManager::UpdatePhaseStats(int32 PhaseIndex)
{
	ACharacter* BossChar = Cast<ACharacter>(OwnerBoss);
	if (!BossChar || !BossChar->GetCharacterMovement())
	{
		return;
	}
	
	if (PhaseWalkSpeeds.IsValidIndex(PhaseIndex))
	{
		float NewSpeed = PhaseWalkSpeeds[PhaseIndex];
		BossChar->GetCharacterMovement()->MaxWalkSpeed = NewSpeed;

		UE_LOG(LogTemp, Log, TEXT("[PatternManager] Phase %d: Walk Speed = %.1f"), 
		       PhaseIndex, NewSpeed);
	}
}

// ========================================
// 유틸리티
// ========================================

void UCBossPatternManager::PlayMontageAndNotify(UAnimMontage* Montage, bool bAutoNotifyFinish)
{
	bShouldNotifyOnMontageEnd = bAutoNotifyFinish;

	if (!OwnerBoss || !Montage)
	{
		if (bAutoNotifyFinish && PhaseComponent)
		{
			GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
			{
				FTimerHandle DelayTimer;
				GetWorld()->GetTimerManager().SetTimer(
					DelayTimer,
					[this]()
					{
						PhaseComponent->NotifyPatternFinished(true);
					},
					1.5f,
					false
				);
			});
		}
		return;
	}
	
	if (UAnimInstance* AnimInst = OwnerBoss->GetMesh()->GetAnimInstance())
	{
		AnimInst->Montage_Play(Montage);
		AnimInst->Montage_SetEndDelegate(MontageEndDelegate, Montage);
	}
}

void UCBossPatternManager::OnMontageCompleted(UAnimMontage* Montage, bool bInterrupted)
{
	if (bShouldNotifyOnMontageEnd && PhaseComponent)
	{
		PhaseComponent->NotifyPatternFinished(!bInterrupted);
	}
}

AActor* UCBossPatternManager::GetPlayerTarget() const
{
	return UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
}

AAIController* UCBossPatternManager::GetBossAI() const
{
	if (!OwnerBoss)
	{
		return nullptr;
	}
	return Cast<AAIController>(OwnerBoss->GetController());
}

// ========================================
// 패턴 종료 알림 (AnimNotify에서 호출)
// ========================================

void UCBossPatternManager::NotifyCurrentPatternEnd(bool bSuccess)
{
	UE_LOG(LogTemp, Warning, TEXT("[PatternManager] ===== PATTERN END: %s ====="), 
		   bSuccess ? TEXT("SUCCESS") : TEXT("FAILED"));
	
	bIsPatternActive = false;
	bIsRushing = false;
	if (OwnerBoss)
	{
		OwnerBoss->SetIsBossRushing(false);
	}
	
	CleanupUtilitySpawnTimers();
		CleanupMinionSpawnTimers();
		CleanupPatternActors();
	
	// 타이머 정리
	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(RushDelayTimer);
		TimerManager.ClearTimer(RushMoveTimer);
	}
	
	// ===== Rush/Barrage만 Chase 재활성화 =====
	if (CurrentPatternId == FName("Rush") || CurrentPatternId == FName("Barrage"))
	{
		if (ABossAIController* BossAI = Cast<ABossAIController>(GetBossAI()))
		{
			BossAI->SetChaseEnabled(true);
			UE_LOG(LogTemp, Warning, TEXT("[PatternManager]  Re-enabled chase after %s in NotifyCurrentPatternEnd"), 
				   *CurrentPatternId.ToString());
		}
	}
	
	if (PhaseComponent)
	{
		PhaseComponent->NotifyPatternFinished(bSuccess);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[PatternManager] PhaseComponent is null!"));
	}
}

void UCBossPatternManager::SpawnPatternActors(const FBossPatternDefinition& PatternData)
{
    SpawnPatternWeapons(PatternData);
    SpawnPatternUtilities(PatternData);
    SpawnPatternMinions(PatternData);
    StartProjectileRain(PatternData.ProjectileRain);
}

void UCBossPatternManager::SpawnPatternWeapons(const FBossPatternDefinition& PatternData)
{
    if (!GetWorld())
    {
        return;
    }

    for (const FBossPatternWeaponSpawnDefinition& Definition : PatternData.WeaponSpawns)
    {
        if (!Definition.WeaponClass)
        {
            continue;
       }

        const FTransform SpawnTransform = ResolveSpawnTransform(Definition.SpawnTransform);
        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = OwnerBoss;
        SpawnParams.Instigator = OwnerBoss;

        AActor* SpawnedActor = GetWorld()->SpawnActor<AActor>(Definition.WeaponClass, SpawnTransform, SpawnParams);
        if (!SpawnedActor)
        {
            continue;
        }

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
            default:
                break;
            }
        }

        RegisterSpawnedActor(SpawnedActor, Definition.bDestroyOnPatternEnd, ActiveWeaponActors);
        ApplyInitialVelocity(SpawnedActor, Definition.InitialVelocity);
    }
}

void UCBossPatternManager::SpawnPatternUtilities(const FBossPatternDefinition& PatternData)
{
    if (!GetWorld())
    {
        return;
    }

    for (const FBossPatternUtilitySpawnDefinition& Definition : PatternData.UtilitySpawns)
    {
        if (!Definition.ActorClass)
        {
            continue;
        }

        const int32 DesiredCount = FMath::Max(Definition.SpawnCount, 1);
        if (Definition.SpawnInterval > KINDA_SMALL_NUMBER && DesiredCount > 1)
        {
            FBossUtilitySpawnRuntime Runtime;
            Runtime.Definition = Definition;

            const int32 RuntimeId = ++UtilitySpawnRuntimeIdCounter;
            ActiveUtilitySpawnRuntimes.Add(RuntimeId, Runtime);

           // 첫 번째 스폰 즉시 처리
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
    if (!GetWorld())
    {
        return;
    }

    for (const FBossPatternMinionSpawnDefinition& Definition : PatternData.MinionSpawns)
    {
        if (!Definition.MinionClass)
        {
            continue;
        }

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
    default:
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
    if (!Actor)
    {
        return;
    }

    FBossSpawnedActorEntry Entry;
    Entry.Actor = Actor;
    Entry.bDestroyOnPatternEnd = bDestroyOnPatternEnd;
    Container.Add(Entry);
}

void UCBossPatternManager::RegisterSpawnedMinion(APawn* Pawn, bool bDestroyOnPatternEnd)
{
    if (!Pawn)
    {
        return;
    }
	
    FBossSpawnedMinionEntry Entry;
    Entry.Pawn = Pawn;
    Entry.bDestroyOnPatternEnd = bDestroyOnPatternEnd;
    ActiveMinions.Add(Entry);
}

void UCBossPatternManager::ApplyInitialVelocity(AActor* SpawnedActor, const FVector& InitialVelocity) const
{
	if (!SpawnedActor || InitialVelocity.IsNearlyZero()){
        return;
    }

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
    if (!GetWorld() || !Definition.ActorClass)
    {
        return;
    }

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
    if (!GetWorld() || !Definition.MinionClass)
    {
        return;
    }

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
    if (!Runtime)
    {
        return;
    }

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
    if (!Runtime)
    {
        return;
    }

    SpawnMinionBatch(Runtime->Definition);

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(Runtime->TimerHandle);
    }
    ActiveMinionSpawnRuntimes.Remove(RuntimeId);
}

void UCBossPatternManager::StartProjectileRain(const FBossPatternProjectileRainSettings& RainSettings)
{
    if (!RainSettings.bEnableRain || !RainSettings.ProjectileClass)
    {
        return;
    }

    if (!GetWorld())
    {
        return;
    }

    StopProjectileRain(false);

    ActiveProjectileRainSettings = RainSettings;
    bProjectileRainActive = true;
    ProjectileRainWaveCounter = 0;

    // 즉시 첫 웨이브 실행
    HandleProjectileRainTick();

    if (!bProjectileRainActive)
    {
        return;
    }

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

void UCBossPatternManager::StopProjectileRain(bool /*bNotifyPatternEnd*/)
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
    if (!GetWorld() || !ActiveProjectileRainSettings.ProjectileClass)
    {
        return;
    }

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
    	FRotator SpawnRotation = ActiveProjectileRainSettings.InitialVelocity.IsNearlyZero() ? FRotator(-90.f, 0.f, 0.f) : ActiveProjectileRainSettings.InitialVelocity.GetSafeNormal().Rotation();
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
