#include "CBossPattern_Rush.h"
#include "00_Character/01_Enemy/CEnemyBossCharacter.h"
#include "00_Character/02_Component/01_EnemyComponent/CEnemyWeaponComponent.h"
#include "00_Character/02_Component/01_EnemyComponent/CBossPatternManager.h"
#include "00_Character/01_Enemy/01_AIController/BossAIController.h"
#include "00_Character/02_Component/00_PlayerComponent/CPlayerKnockbackComponent.h"
#include "AIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Animation/AnimInstance.h"
#include "DrawDebugHelpers.h"

UCBossPattern_Rush::UCBossPattern_Rush()
{
	PatternId = FName("Rush");
}

void UCBossPattern_Rush::BeginDestroy()
{
	UE_LOG(LogTemp, Log, TEXT("[Rush] BeginDestroy called"));
	Cleanup();
	Super::BeginDestroy();
}

void UCBossPattern_Rush::ExecutePattern(int32 PhaseIndex, const FBossPatternDefinition& PatternData)
{
	Super::ExecutePattern(PhaseIndex, PatternData);

	if (!HasValidOwner())
		return;

	UE_LOG(LogTemp, Warning, TEXT("[Rush] ========================================"));
	UE_LOG(LogTemp, Warning, TEXT("[Rush] Executing rush attack - Phase %d"), PhaseIndex);
	UE_LOG(LogTemp, Warning, TEXT("[Rush] ExecutionTime: %.2f, RecoveryTime: %.2f, Cooldown: %.2f"), 
		PatternData.ExecutionTime, PatternData.RecoveryTime, PatternData.Cooldown);
	UE_LOG(LogTemp, Warning, TEXT("[Rush] Last Used Time: %.1fs"), LastUsedTime);
	
	if (OwnerBoss->GetWorld())
	{
		float CurrentTime = OwnerBoss->GetWorld()->GetTimeSeconds();
		float TimeSinceLastUse = CurrentTime - LastUsedTime;
		UE_LOG(LogTemp, Warning, TEXT("[Rush] Time Since Last Use: %.1fs"), TimeSinceLastUse);
	}
	UE_LOG(LogTemp, Warning, TEXT("[Rush] ========================================"));

	// PatternData 저장
	CurrentPatternData = PatternData;

	if (ABossAIController* BossAI = Cast<ABossAIController>(GetBossAI()))
	{
		BossAI->SetChaseEnabled(false);
	}

	ResetTransientData();
	BeginTelegraphInternal();
}

void UCBossPattern_Rush::OnPatternEnd()
{
	UE_LOG(LogTemp, Log, TEXT("[Rush] Pattern ended"));

	if (ABossAIController* BossAI = Cast<ABossAIController>(GetBossAI()))
	{
		BossAI->SetChaseEnabled(true);
	}

	if (OwnerBoss.IsValid() && OwnerBoss->GetCharacterMovement())
	{
		OwnerBoss->GetCharacterMovement()->MaxWalkSpeed = SavedMaxWalkSpeed;
	}

	if (OwnerBoss.IsValid())
	{
		OwnerBoss->SetIsBossRushing(false);
	}
	
	Super::OnPatternEnd();
	EnterState(ERushState::Idle);
	
}

void UCBossPattern_Rush::Cleanup()
{
	Super::Cleanup();
	
	if (!HasValidOwner())
		return;
	
	ClearTimers();
	ResetTransientData();
}

// ─────────────────────────────────────────────────────────────
// State Management
// ─────────────────────────────────────────────────────────────

void UCBossPattern_Rush::EnterState(ERushState NewState)
{
	if (State == NewState)
		return;
	
	ERushState PrevState = State;
	State = NewState;
	
	// 상태 이름 출력
	const TCHAR* StateNames[] = {TEXT("Idle"), TEXT("Telegraph"), TEXT("Rushing"), TEXT("Recovery"), TEXT("Cooldown")};
	UE_LOG(LogTemp, Warning, TEXT("[Rush] State Changed: %s -> %s"), 
		StateNames[(int32)PrevState], StateNames[(int32)NewState]);
	
	OnRushStateChanged.Broadcast(NewState, PrevState);
}

void UCBossPattern_Rush::ResetTransientData()
{
	LockedRushDirection = FVector::ForwardVector;
	bDirectionLocked = false;
	RushStartTime = 0.f;
	DamagedPlayers.Empty();
	LastEndReason = ERushEndReason::None; 
}

void UCBossPattern_Rush::ClearTimers()
{
	if (!HasValidOwner()) return;
	UWorld* World = OwnerBoss->GetWorld();
	if (!World) return;

	FTimerManager& TM = World->GetTimerManager();
	TM.ClearTimer(TH_Telegraph);
	TM.ClearTimer(TH_MaxRush);
	TM.ClearTimer(TH_Recovery);
}

// ─────────────────────────────────────────────────────────────
// Telegraph Phase
// ─────────────────────────────────────────────────────────────

void UCBossPattern_Rush::BeginTelegraphInternal()
{
	EnterState(ERushState::Telegraph);
	if (!HasValidOwner()) return;

	if (AActor* Player = GetPlayerTarget())
	{
		const FVector Direction = (Player->GetActorLocation() - OwnerBoss->GetActorLocation()).GetSafeNormal2D();
		
		if (!Direction.IsNearlyZero())
		{
			LockedRushDirection = Direction;
			bDirectionLocked = true;
			
			OwnerBoss->SetActorRotation(Direction.Rotation());
			
			UE_LOG(LogTemp, Warning, TEXT("[Rush] Direction LOCKED at Telegraph: %s"), *LockedRushDirection.ToString());
		}
	}

	if (TelegraphMontage && OwnerBoss->GetMesh())
	{
		if (UAnimInstance* AnimInstance = OwnerBoss->GetMesh()->GetAnimInstance())
		{
			AnimInstance->Montage_Play(TelegraphMontage);
		}
	}

	if (bAutoStartOnTelegraphEnd)
	{
		UWorld* World = OwnerBoss->GetWorld();
		if (World)
		{
			// ExecutionTime의 일부를 Telegraph로 사용 (20% 또는 최소 0.5초)
			float TelegraphTime = FMath::Max(0.5f, CurrentPatternData.ExecutionTime * 0.2f);
			World->GetTimerManager().SetTimer(TH_Telegraph, this, &UCBossPattern_Rush::Anim_RushStart, TelegraphTime, false);
			UE_LOG(LogTemp, Log, TEXT("[Rush] Telegraph duration: %.2fs"), TelegraphTime);
		}
	}
}

// ─────────────────────────────────────────────────────────────
// Rushing Phase
// ─────────────────────────────────────────────────────────────

void UCBossPattern_Rush::Anim_RushStart()
{
	if (State != ERushState::Telegraph)
		return;
	BeginRushingInternal();
}

void UCBossPattern_Rush::BeginRushingInternal()
{
	EnterState(ERushState::Rushing);
	if (!HasValidOwner())
		return;

	UE_LOG(LogTemp, Warning, TEXT("[Rush] Starting rush movement!"));

	// Telegraph에서 이미 고정된 방향을 사용
	if (!bDirectionLocked || LockedRushDirection.IsNearlyZero())
	{
		UE_LOG(LogTemp, Error, TEXT("[Rush] Direction not locked! Aborting rush."));
		HandlePatternComplete();
		return;
	}
	
	UE_LOG(LogTemp, Warning, TEXT("[Rush] Using LOCKED direction: %s"), *LockedRushDirection.ToString());

	if (UCharacterMovementComponent* Movement = OwnerBoss->GetCharacterMovement())
	{
		SavedMaxWalkSpeed = Movement->MaxWalkSpeed;
		SavedMaxAcceleration = Movement->MaxAcceleration;
		SavedBrakingDeceleration = Movement->BrakingDecelerationWalking;
		SavedGroundFriction = Movement->GroundFriction;
		bSavedOrientRotationToMovement = Movement->bOrientRotationToMovement;
		
		UE_LOG(LogTemp, Log, TEXT("[Rush]  Saved Movement Settings: Speed=%.1f, Accel=%.1f, Braking=%.1f"), 
			SavedMaxWalkSpeed, SavedMaxAcceleration, SavedBrakingDeceleration);
		
		Movement->SetMovementMode(MOVE_Walking);
		Movement->MaxWalkSpeed = RushSpeed;
		Movement->MaxAcceleration = 10000.0f;
		Movement->BrakingDecelerationWalking = 0.0f;
		Movement->GroundFriction = 0.0f;
		Movement->bOrientRotationToMovement = false;  // 돌진 방향 고정
		
		UE_LOG(LogTemp, Log, TEXT("[Rush] Rush Movement Settings Applied: Speed=%.1f"), RushSpeed);
	}

	if (RushMontage && OwnerBoss->GetMesh())
	{
		if (UAnimInstance* AnimInstance = OwnerBoss->GetMesh()->GetAnimInstance())
		{
			AnimInstance->Montage_Play(RushMontage);
		}
	}

	OwnerBoss->SetIsBossRushing(true);
	
	UWorld* World = OwnerBoss->GetWorld();
	RushStartTime = World ? World->GetTimeSeconds() : 0.f;

	if (World)
	{
		World->GetTimerManager().SetTimer(TH_MaxRush, this, &UCBossPattern_Rush::HandleMaxRushTime, MaxRushTime, false);
	}
}

void UCBossPattern_Rush::TickRushMovement(float DeltaTime)
{
	if (State != ERushState::Rushing || !HasValidOwner()) return;
	UpdateRushing(DeltaTime);
}

void UCBossPattern_Rush::UpdateRushing(float DeltaSeconds)
{
	if (!HasValidOwner()) return;

	static float LogTimer = 0.f;
	LogTimer += DeltaSeconds;
	
	if (LogTimer >= 0.5f)
	{
		LogTimer = 0.f;
		FVector CurrentVelocity = OwnerBoss->GetVelocity();
		
		UE_LOG(LogTemp, Log, TEXT("[Rush] Rushing... Velocity: %.1f, Direction: %s"), 
			CurrentVelocity.Size(), *LockedRushDirection.ToString());
	}

	// 매 프레임 Overlap 체크
	CheckOverlappingActors();
	if (State != ERushState::Rushing) return;

	// 충돌 감지 (Sweep)
	PerformCollisionTrace();
	if (State != ERushState::Rushing) return;

	float CurrentTime = OwnerBoss->GetWorld()->GetTimeSeconds();
	if (CurrentTime - RushStartTime >= RushMissTimeout) 
	{
		UE_LOG(LogTemp, Warning, TEXT("[Rush] Player Missed (Timeout: %.2f), ending rush."), RushMissTimeout);
		EndRushingInternal(ERushEndReason::MaxTime, nullptr);
		return; 
	}
	
	// 고정된 방향으로 이동
	if (!LockedRushDirection.IsNearlyZero())
	{
		OwnerBoss->AddMovementInput(LockedRushDirection, 1.0f);
	}
}

void UCBossPattern_Rush::PerformCollisionTrace()
{
	if (!HasValidOwner())
		return;

	FHitResult HitResult;
	bool bHit = SweepAhead(HitResult, CollisionTraceAhead);
	
	if (!bHit)
		return;

	AActor* HitActor = HitResult.GetActor();
	if (!HitActor)
		return;

	if (HitResult.Component.IsValid() && HitResult.Component->GetCollisionObjectType() == ECC_WorldStatic)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Rush] Hit Wall: %s"), *HitActor->GetName());
		EndRushingInternal(ERushEndReason::MaxTime, HitActor); // 벽에 박으면 종료
		return;
	}

	TWeakObjectPtr<AActor> WeakHit = HitActor;
	if (DamagedPlayers.Contains(WeakHit))
	{
		UE_LOG(LogTemp, Log, TEXT("[Rush] Already damaged: %s"), *HitActor->GetName());
		return;
	}
	
	if (HitActor == OwnerBoss.Get())
		return;

	ACharacter* HitCharacter = Cast<ACharacter>(HitActor);
	if (!HitCharacter)
		return;

	AController* HitController = HitCharacter->GetController();
	if (Cast<AAIController>(HitController))
		return;

	UE_LOG(LogTemp, Warning, TEXT("[Rush] Hit Player: %s"), *HitActor->GetName());

	// 데미지 적용
	float ActualDamage = UGameplayStatics::ApplyDamage
	(
		HitActor, 
		RushDamage, 
		OwnerBoss->GetController(), 
		OwnerBoss.Get(), 
		nullptr
	);
	UE_LOG(LogTemp, Warning, TEXT("[Rush] Damage Applied: %.1f"), ActualDamage);

	FVector KnockDirection = LockedRushDirection;
	if (KnockDirection.IsNearlyZero())
	{
		// Fallback: 보스에서 플레이어 방향으로
		KnockDirection = (HitCharacter->GetActorLocation() - OwnerBoss->GetActorLocation()).GetSafeNormal2D();
		if (KnockDirection.IsNearlyZero())
		{
			KnockDirection = FVector::ForwardVector;
		}
	}

	FVector LaunchVelocity = KnockDirection * RushLaunchPower;
	LaunchVelocity.Z += RushLaunchUp;
	HitCharacter->LaunchCharacter(LaunchVelocity, true, true);
	
	UE_LOG(LogTemp, Warning, TEXT("[Rush] Launch Applied: Power=%.1f, Up=%.1f, Direction=%s"), 
		RushLaunchPower, RushLaunchUp, *KnockDirection.ToString());

	if (UCPlayerKnockbackComponent* KnockbackComp = HitCharacter->FindComponentByClass<UCPlayerKnockbackComponent>())
	{
		KnockbackComp->StartKnockback(OwnerBoss.Get());
		UE_LOG(LogTemp, Warning, TEXT("[Rush] Knockback Component Triggered"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Rush]  No Knockback Component Found"));
	}

	// 중복 데미지 방지를 위해 리스트에 추가
	DamagedPlayers.Add(WeakHit);
	
	// 플레이어와 충돌했으므로 돌진 종료
	EndRushingInternal(ERushEndReason::HitPlayer, HitActor);
}

bool UCBossPattern_Rush::SweepAhead(FHitResult& OutHit, float Distance) const
{
	if (!HasValidOwner())
		return false;
	
	FVector Start = OwnerBoss->GetActorLocation();
	FVector End = Start + OwnerBoss->GetActorForwardVector() * Distance;
	
	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(OwnerBoss.Get());
	
	EDrawDebugTrace::Type DebugType = (State == ERushState::Rushing) ? EDrawDebugTrace::ForOneFrame : EDrawDebugTrace::None;
	
	bool bHit = UKismetSystemLibrary::SphereTraceSingle(
		OwnerBoss->GetWorld(), 
		Start, 
		End, 
		CollisionRadius, 
		UEngineTypes::ConvertToTraceType(ECC_Pawn), 
		false, 
		ActorsToIgnore, 
		DebugType, 
		OutHit, 
		true
	);
	
	if (bHit && State == ERushState::Rushing)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Rush] SweepAhead HIT: %s at distance %.1f"), 
			*GetNameSafe(OutHit.GetActor()), OutHit.Distance);
	}
	
	return bHit;
}


// ─────────────────────────────────────────────────────────────
// Overlap Detection (매 프레임 체크)
// ─────────────────────────────────────────────────────────────

void UCBossPattern_Rush::CheckOverlappingActors()
{
	if (!HasValidOwner()) return;

	UCapsuleComponent* Capsule = OwnerBoss->GetCapsuleComponent();
	if (!Capsule) return;

	TArray<AActor*> OverlappingActors;
	Capsule->GetOverlappingActors(OverlappingActors);

	for (AActor* OtherActor : OverlappingActors)
	{
		if (!OtherActor || OtherActor == OwnerBoss.Get())
		{
			continue;
		}

		// 중복 데미지 방지
		TWeakObjectPtr<AActor> WeakOther = OtherActor;
		if (DamagedPlayers.Contains(WeakOther))
		{
			continue;
		}

		ACharacter* HitCharacter = Cast<ACharacter>(OtherActor);
		if (!HitCharacter)
		{
			continue;
		}

		AController* HitController = HitCharacter->GetController();
		if (Cast<AAIController>(HitController))
		{
			continue;
		}

		UE_LOG(LogTemp, Warning, TEXT("[Rush] OVERLAP DETECTED Player: %s"), *OtherActor->GetName());

		// 데미지 적용
		float ActualDamage = UGameplayStatics::ApplyDamage
		(
			OtherActor, 
			RushDamage, 
			OwnerBoss->GetController(), 
			OwnerBoss.Get(), 
			nullptr
		);
		UE_LOG(LogTemp, Warning, TEXT("[Rush] OVERLAP Damage Applied: %.1f"), ActualDamage);

		// 넉백 방향 계산
		FVector KnockDirection = LockedRushDirection;
		if (KnockDirection.IsNearlyZero())
		{
			KnockDirection = (HitCharacter->GetActorLocation() - OwnerBoss->GetActorLocation()).GetSafeNormal2D();
			if (KnockDirection.IsNearlyZero())
			{
				KnockDirection = FVector::ForwardVector;
			}
		}

		FVector LaunchVelocity = KnockDirection * RushLaunchPower;
		LaunchVelocity.Z += RushLaunchUp;
		HitCharacter->LaunchCharacter(LaunchVelocity, true, true);
		
		UE_LOG(LogTemp, Warning, TEXT("[Rush] OVERLAP Launch: Power=%.1f, Up=%.1f"), 
			RushLaunchPower, RushLaunchUp);

		if (UCPlayerKnockbackComponent* KnockbackComp = HitCharacter->FindComponentByClass<UCPlayerKnockbackComponent>())
		{
			KnockbackComp->StartKnockback(OwnerBoss.Get());
			UE_LOG(LogTemp, Warning, TEXT("[Rush] OVERLAP Knockback Component Triggered"));
		}

		// 중복 데미지 방지
		DamagedPlayers.Add(WeakOther);

		EndRushingInternal(ERushEndReason::HitPlayer, OtherActor);
		return; 
	}
}

void UCBossPattern_Rush::HandleMaxRushTime()
{
	if (State == ERushState::Rushing)
	{
		EndRushingInternal(ERushEndReason::MaxTime, nullptr);
	}
}

void UCBossPattern_Rush::EndRushingInternal(ERushEndReason Reason, AActor* HitActor)
{
	if (State != ERushState::Rushing)
		return;

	// 종료 사유 로그
	LastEndReason = Reason;
	
	const TCHAR* ReasonNames[] = {TEXT("None"), TEXT("ReachedTarget"), TEXT("HitPlayer"), TEXT("MaxTime"), TEXT("Aborted")};
	UE_LOG(LogTemp, Warning, TEXT("[Rush] Ending Rush - Reason: %s, HitActor: %s"), 
		ReasonNames[(int32)Reason], *GetNameSafe(HitActor));

	if (HasValidOwner())
	{
		UWorld* World = OwnerBoss->GetWorld();
		if (World) World->GetTimerManager().ClearTimer(TH_MaxRush);
	}

	if (OwnerBoss.IsValid())
		OwnerBoss->SetIsBossRushing(false);

	OnRushFinished.Broadcast(Reason, HitActor);
	BeginRecoveryInternal(Reason, HitActor);
}

// ─────────────────────────────────────────────────────────────
// Recovery Phase
// ─────────────────────────────────────────────────────────────

void UCBossPattern_Rush::BeginRecoveryInternal(ERushEndReason Reason, AActor* HitActor)
{
	UE_LOG(LogTemp, Warning, TEXT("[Rush] ⚡ BeginRecoveryInternal called"));
	
	EnterState(ERushState::Recovery);
	
	if (!HasValidOwner())
		return;

	// 즉시 정지 - Velocity를 직접 0으로 설정
	if (UCharacterMovementComponent* Movement = OwnerBoss->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->Velocity = FVector::ZeroVector;
		
		// 스냅샷 복구
		Movement->MaxWalkSpeed = SavedMaxWalkSpeed;
		Movement->MaxAcceleration = SavedMaxAcceleration;
		Movement->BrakingDecelerationWalking = SavedBrakingDeceleration;
		Movement->GroundFriction = SavedGroundFriction;
		Movement->bOrientRotationToMovement = bSavedOrientRotationToMovement;
		
		UE_LOG(LogTemp, Warning, TEXT("[Rush] Movement RESTORED: Speed=%.1f, Accel=%.1f, Braking=%.1f"), 
			SavedMaxWalkSpeed, SavedMaxAcceleration, SavedBrakingDeceleration);
	}

	if (OwnerBoss->GetMesh())
	{
		if (UAnimInstance* AnimInstance = OwnerBoss->GetMesh()->GetAnimInstance())
			{
			// 모든 몽타주 즉시 정지
			AnimInstance->StopAllMontages(0.0f);
			
			if (RecoveryMontage)
			{
				float PlayRate = AnimInstance->Montage_Play(RecoveryMontage, 1.0f);
				UE_LOG(LogTemp, Warning, TEXT("[Rush] Playing Recovery Montage: %s (PlayRate: %.2f)"), 
					*RecoveryMontage->GetName(), PlayRate);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[Rush]  RecoveryMontage is NULL! Cannot play recovery animation!"));
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[Rush] AnimInstance is NULL!"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[Rush] Mesh is NULL!"));
	}

	UWorld* World = OwnerBoss->GetWorld();
	if (World) {
		World->GetTimerManager().SetTimer(TH_Recovery, this, &UCBossPattern_Rush::HandlePatternComplete, CurrentPatternData.RecoveryTime, false);
		UE_LOG(LogTemp, Log, TEXT("[Rush] Recovery duration: %.2fs"), CurrentPatternData.RecoveryTime);
	}
}

void UCBossPattern_Rush::Anim_RecoveryEnd()
{
	if (State == ERushState::Recovery)
	{
		HandlePatternComplete();
	}
}

// ─────────────────────────────────────────────────────────────
// Deprecated -> Updated Compatibility Methods
// ─────────────────────────────────────────────────────────────

void UCBossPattern_Rush::HandleRushMovementStart()
{
	if (!IsOnCooldown())
	{
		return;
	}
	
	if (State == ERushState::Telegraph)
	{
		Anim_RushStart();
	}
}

void UCBossPattern_Rush::HandleRushMovementStop()
{
	UE_LOG(LogTemp, Warning, TEXT("[Rush] HandleRushMovementStop called from Manager (Force Stop)"));

	if (State == ERushState::Recovery || State == ERushState::Cooldown || State == ERushState::Idle)
	{
		return;
	}

	BeginRecoveryInternal(ERushEndReason::MaxTime, nullptr);
}

// ─────────────────────────────────────────────────────────────
// Helper Functions
// ─────────────────────────────────────────────────────────────

void UCBossPattern_Rush::HandlePatternComplete()
{
	if (OwnerBoss.IsValid() && OwnerBoss->GetWorld())
	{
		OwnerBoss->GetWorld()->GetTimerManager().ClearTimer(TH_Recovery);
	}

	if (State == ERushState::Idle)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Rush] HandlePatternComplete ignored - already in Idle (forced stop)"));
		return;
	}

	if (State == ERushState::Cooldown)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Rush] HandlePatternComplete ignored - already in Cooldown"));
		return;
	}

	EnterState(ERushState::Cooldown);
	
	bool bApplyCooldown = true;
	
	if (LastEndReason == ERushEndReason::MaxTime)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Rush]  MISSED Player - Ending without cooldown"));
		bApplyCooldown = true;
	}
	else if (LastEndReason == ERushEndReason::HitPlayer)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Rush] HIT Player - Ending with cooldown"));
		bApplyCooldown = true;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Rush] Other reason - Ending without cooldown"));
		bApplyCooldown = true;
	}
	
	FinishPattern(bApplyCooldown);
}


bool UCBossPattern_Rush::HasValidOwner() const
{
	return OwnerBoss.IsValid() && OwnerBoss->GetWorld() != nullptr;
}