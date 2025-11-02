#include "CBossPatternManager.h"

#include "00_Character/01_Enemy/CEnemyBossCharacter.h"
#include "00_Character/02_Component/01_EnemyComponent/CEnemyBossPhaseComponent.h"
#include "00_Character/02_Component/01_EnemyComponent/CEnemyWeaponComponent.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "Camera/CameraShakeBase.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"

UCBossPatternManager::UCBossPatternManager()
{
	PrimaryComponentTick.bCanEverTick = false;
	bIsPatternActive = false;
	bShouldNotifyOnMontageEnd = false;
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
		TimerManager.ClearTimer(TeleportTimer);
		TimerManager.ClearTimer(PhaseTransitionTimer);
	}

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

	CurrentPatternId = PatternId;
	bIsPatternActive = true;
	bIsRushing = false; // 러쉬 플래그 초기화
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
	else if (PatternId == FName("GroundPound"))
	{
		ExecuteGroundPoundPattern();
	}
	else if (PatternId == FName("Teleport"))
	{
		ExecuteTeleportPattern();
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

	// 패턴별 정리 작업
	if (PatternId == FName("Rush"))
	{
		GetWorld()->GetTimerManager().ClearTimer(RushMoveTimer);
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
	UE_LOG(LogTemp, Warning, TEXT("[PatternManager] RUSH ATTACK (In-Place AnimNotify)!"));

	if (!WeaponComponent)
	{
		PhaseComponent->NotifyPatternFinished(false);
		return;
	}

	// 1. WeaponComponent에 돌진 공격 인덱스 설정 및 몽타주 재생 요청
	if (const int32* AttackIndex = PatternAttackIndexMap.Find(FName("Rush")))
	{
		// 이동 속도를 RushSpeed로 설정
		if (OwnerBoss && OwnerBoss->GetCharacterMovement())
		{
			OwnerBoss->GetCharacterMovement()->MaxWalkSpeed = RushSpeed;
		}
		
		WeaponComponent->SetCurrentAttackIndex(*AttackIndex);
		WeaponComponent->DoAttack(); // 몽타주 재생 시작
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[PatternManager] Rush pattern index not found in map!"));
		PhaseComponent->NotifyPatternFinished(false);
		return;
	}

	// 2. 재생된 몽타주가 끝나면 자동으로 패턴 완료를 알리도록 설정
	PlayMontageAndNotify(nullptr, true);
}

void UCBossPatternManager::HandleRushMovementStart()
{
	UE_LOG(LogTemp, Log, TEXT("[PatternManager] AnimNotify: StartRushMovement received!"));
	bIsRushing = true;
}

void UCBossPatternManager::HandleRushMovementStop()
{
	UE_LOG(LogTemp, Log, TEXT("[PatternManager] AnimNotify: StopRushMovement received!"));
	bIsRushing = false;

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

	if (PhaseComponent)
	{
		PhaseComponent->NotifyPatternFinished(true);
	}
}

// ========================================
// 패턴 실행: Ground Pound
// ========================================

void UCBossPatternManager::ExecuteGroundPoundPattern()
{
	UE_LOG(LogTemp, Warning, TEXT("[PatternManager]  GROUND POUND!"));

	// 캐릭터를 공중으로 발사
	if (ACharacter* BossChar = Cast<ACharacter>(OwnerBoss))
	{
		FVector LaunchVelocity(0.f, 0.f, GroundPoundLaunchPower);
		BossChar->LaunchCharacter(LaunchVelocity, false, true);

		// 착지 확인 타이머
		GetWorld()->GetTimerManager().SetTimer(
			GroundCheckTimer,
			this,
			&UCBossPatternManager::CheckGroundPoundLanding,
			0.1f,
			true
		);
	}
	else
	{
		PhaseComponent->NotifyPatternFinished(false);
	}
}

void UCBossPatternManager::CheckGroundPoundLanding()
{
	ACharacter* BossChar = Cast<ACharacter>(OwnerBoss);
	if (!BossChar)
	{
		return;
	}

	// 착지 확인
	if (!BossChar->GetCharacterMovement()->IsFalling())
	{
		GetWorld()->GetTimerManager().ClearTimer(GroundCheckTimer);
		OnGroundPoundLanded();
	}
}

void UCBossPatternManager::OnGroundPoundLanded()
{
	UE_LOG(LogTemp, Warning, TEXT("[PatternManager] Ground Pound IMPACT!"));

	FVector ImpactLocation = OwnerBoss->GetActorLocation();

	// 이펙트
	if (GroundImpactEffect)
	{
		UGameplayStatics::SpawnEmitterAtLocation(
			GetWorld(),
			GroundImpactEffect,
			ImpactLocation,
			FRotator::ZeroRotator,
			FVector(3.f)
		);
	}

	// 사운드
	if (GroundImpactSound)
	{
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), GroundImpactSound, ImpactLocation);
	}

	// 카메라 흔들림
	if (GroundImpactShake)
	{
		UGameplayStatics::PlayWorldCameraShake(
			GetWorld(),
			GroundImpactShake,
			ImpactLocation,
			0.f,
			2000.f
		);
	}

	// AOE 데미지
	UGameplayStatics::ApplyRadialDamage(
		GetWorld(),
		GroundPoundDamage,
		ImpactLocation,
		GroundPoundDamageRadius,
		UDamageType::StaticClass(),
		TArray<AActor*>(),
		OwnerBoss,
		GetBossAI(),
		true
	);

	// 패턴 완료
	if (PhaseComponent)
	{
		PhaseComponent->NotifyPatternFinished(true);
	}
}

// ========================================
// 패턴 실행: Teleport
// ========================================

void UCBossPatternManager::ExecuteTeleportPattern()
{
	UE_LOG(LogTemp, Warning, TEXT("[PatternManager]  TELEPORT!"));

	// 사라지기 이펙트
	if (TeleportOutEffect)
	{
		UGameplayStatics::SpawnEmitterAtLocation(
			GetWorld(),
			TeleportOutEffect,
			OwnerBoss->GetActorLocation()
		);
	}

	if (TeleportSound)
	{
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), TeleportSound, OwnerBoss->GetActorLocation());
	}

	// 보스 숨기기
	OwnerBoss->SetActorHiddenInGame(true);
	OwnerBoss->SetActorEnableCollision(false);

	// 0.5초 후 텔레포트
	GetWorld()->GetTimerManager().SetTimer(
		TeleportTimer,
		[this]()
		{
			// 새 위치 계산
			FVector NewLocation = GetRandomNavigablePoint(TeleportMaxDistance);

			// 이동
			OwnerBoss->SetActorLocation(NewLocation);

			// 플레이어 바라보기
			if (AActor* Target = GetPlayerTarget())
			{
				FRotator LookRotation = (Target->GetActorLocation() - NewLocation).Rotation();
				OwnerBoss->SetActorRotation(FRotator(0, LookRotation.Yaw, 0));
			}

			// 0.3초 후 나타나기
			GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
			{
				OwnerBoss->SetActorHiddenInGame(false);
				OwnerBoss->SetActorEnableCollision(true);

				// 나타나기 이펙트
				if (TeleportInEffect)
				{
					UGameplayStatics::SpawnEmitterAtLocation(
						GetWorld(),
						TeleportInEffect,
						OwnerBoss->GetActorLocation()
					);
				}

				if (PhaseComponent)
				{
					PhaseComponent->NotifyPatternFinished(true);
				}
			});
		},
		0.5f,
		false
	);
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

	// 페이즈별 속도 조정
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
		// 몽타주가 없으면 1.5초 딜레이 후 자동 완료
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

	// 몽타주 재생
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

FVector UCBossPatternManager::GetRandomNavigablePoint(float Radius) const
{
	if (!OwnerBoss)
	{
		return FVector::ZeroVector;
	}

	AActor* Target = GetPlayerTarget();
	FVector CenterPoint = Target ? Target->GetActorLocation() : OwnerBoss->GetActorLocation();

	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!NavSys)
	{
		return CenterPoint;
	}

	FNavLocation NavLocation;
	float MinDistance = FMath::Max(TeleportMinDistance, Radius * 0.5f);
	float MaxDistance = Radius;

	// 랜덤 방향
	FVector RandomDirection = FMath::VRand();
	RandomDirection.Z = 0.f;
	RandomDirection.Normalize();

	float RandomDistance = FMath::RandRange(MinDistance, MaxDistance);
	FVector TestPoint = CenterPoint + (RandomDirection * RandomDistance);

	// 네비메시에서 가장 가까운 점 찾기
	if (NavSys->ProjectPointToNavigation(TestPoint, NavLocation, FVector(500.f, 500.f, 500.f)))
	{
		return NavLocation.Location;
	}

	return OwnerBoss->GetActorLocation();
}