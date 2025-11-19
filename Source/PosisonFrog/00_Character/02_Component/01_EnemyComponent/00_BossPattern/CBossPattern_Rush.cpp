#include "CBossPattern_Rush.h"
#include "00_Character/01_Enemy/CEnemyBossCharacter.h"
#include "00_Character/02_Component/01_EnemyComponent/CEnemyWeaponComponent.h"
#include "00_Character/02_Component/01_EnemyComponent/CBossPatternManager.h"
#include "00_Character/01_Enemy/01_AIController/BossAIController.h"
#include "00_Character/02_Component/00_PlayerComponent/CPlayerKnockbackComponent.h"
#include "AIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Animation/AnimInstance.h"
#include "DrawDebugHelpers.h"

UCBossPattern_Rush::UCBossPattern_Rush()
{
	PatternId = FName("Rush");
	CurrentTelegraphDuration = Phase1_TelegraphDuration;
	CurrentRecoveryDuration = Phase1_RecoveryDuration;
}

void UCBossPattern_Rush::BeginDestroy()
{
	UE_LOG(LogTemp, Log, TEXT("[Rush] BeginDestroy called"));
	Cleanup();
	Super::BeginDestroy();
}

void UCBossPattern_Rush::ExecutePattern(int32 PhaseIndex)
{
	Super::ExecutePattern(PhaseIndex);

	if (!HasValidOwner())
		return;

	UE_LOG(LogTemp, Warning, TEXT("[Rush] Executing rush attack - Phase %d"), PhaseIndex);

	if (ABossAIController* BossAI = Cast<ABossAIController>(GetBossAI()))
	{
		BossAI->SetChaseEnabled(false);
	}

	ResetTransientData();
	BeginTelegraphInternal();
}

void UCBossPattern_Rush::OnPatternEnd()
{
	Super::OnPatternEnd();
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

void UCBossPattern_Rush::UpdatePhaseSettings(int32 PhaseIndex)
{
	Super::UpdatePhaseSettings(PhaseIndex);
	if (PhaseIndex == 0)
	{
		CurrentTelegraphDuration = Phase1_TelegraphDuration;
		CurrentRecoveryDuration = Phase1_RecoveryDuration;
	}
	else if (PhaseIndex >= 1)
	{
		CurrentTelegraphDuration = Phase2_TelegraphDuration;
		CurrentRecoveryDuration = Phase2_RecoveryDuration;
	}
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
	RushTargetLocation = FVector::ZeroVector;
	RushDirection = FVector::ForwardVector;
	RushStartTime = 0.f;
	DamagedPlayers.Empty();
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
		RushTargetLocation = Player->GetActorLocation();
		const FVector Direction = (RushTargetLocation - OwnerBoss->GetActorLocation()).GetSafeNormal2D();
		
		if (!Direction.IsNearlyZero())
		{
			OwnerBoss->SetActorRotation(Direction.Rotation());
			RushDirection = Direction;
			UE_LOG(LogTemp, Warning, TEXT("[Rush] Direction locked at Telegraph: %s"), *RushDirection.ToString());
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
			World->GetTimerManager().SetTimer(TH_Telegraph, this, &UCBossPattern_Rush::Anim_RushStart, CurrentTelegraphDuration, false);
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

	if (AActor* Player = GetPlayerTarget())
	{
		RushTargetLocation = Player->GetActorLocation();
		const FVector Direction = (RushTargetLocation - OwnerBoss->GetActorLocation()).GetSafeNormal2D();
		if (!Direction.IsNearlyZero())
		{
			OwnerBoss->SetActorRotation(Direction.Rotation());
			RushDirection = Direction;
			UE_LOG(LogTemp, Warning, TEXT("[Rush] Final direction locked at Rush Start: %s"), *RushDirection.ToString());
		}
	}

	if (UCharacterMovementComponent* Movement = OwnerBoss->GetCharacterMovement())
	{
		SavedMaxWalkSpeed = Movement->MaxWalkSpeed;
		SavedMaxAcceleration = Movement->MaxAcceleration;
		SavedBrakingDeceleration = Movement->BrakingDecelerationWalking;
		SavedGroundFriction = Movement->GroundFriction;
		
		Movement->SetMovementMode(MOVE_Walking);
		Movement->MaxWalkSpeed = RushSpeed;
		Movement->MaxAcceleration = 10000.0f;
		Movement->BrakingDecelerationWalking = 0.0f;
		Movement->GroundFriction = 0.0f;
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

	// 주기적 상태 로그 (0.5초마다)
	static float LogTimer = 0.f;
	LogTimer += DeltaSeconds;
	
	if (LogTimer >= 0.5f)
	{
		LogTimer = 0.f;
		const float Distance = DistanceToTarget2D();
		FVector CurrentVelocity = OwnerBoss->GetVelocity();
		
		UE_LOG(LogTemp, Log, TEXT("[Rush] Rushing... Distance: %.1f, Velocity: %.1f, Direction: %s"), 
			Distance, CurrentVelocity.Size(), *RushDirection.ToString());
	}

	PerformCollisionTrace();

	const float Distance = DistanceToTarget2D();
	if (Distance <= RushAcceptanceRadius)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Rush] Reached target location!"));
		EndRushingInternal(ERushEndReason::ReachedTarget);
		return;
	}

	if (!RushDirection.IsNearlyZero())
	{
		OwnerBoss->AddMovementInput(RushDirection, 1.0f);
	}
}

void UCBossPattern_Rush::PerformCollisionTrace()
{
	if (!HasValidOwner()) return;

	FHitResult HitResult;
	bool bHit = SweepAhead(HitResult, CollisionTraceAhead);
	
	if (!bHit) return;

	AActor* HitActor = HitResult.GetActor();
	if (!HitActor) return;

	if (HitResult.Component.IsValid() && HitResult.Component->GetCollisionObjectType() == ECC_WorldStatic)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Rush] Hit World, ending rush"));
		EndRushingInternal(ERushEndReason::HitPlayer, HitActor);
		return;
	}

	TWeakObjectPtr<AActor> WeakHit = HitActor;
	if (DamagedPlayers.Contains(WeakHit)) return;
	
	if (HitActor == OwnerBoss.Get()) return;

	ACharacter* HitCharacter = Cast<ACharacter>(HitActor);
	if (!HitCharacter) return;

	AController* HitController = HitCharacter->GetController();
	if (Cast<AAIController>(HitController)) return;

	UE_LOG(LogTemp, Warning, TEXT("[Rush]  Hit: %s"), *HitActor->GetName());

	float ActualDamage = UGameplayStatics::ApplyDamage
	(
		HitActor, 
		RushDamage, 
		OwnerBoss->GetController(), 
		OwnerBoss.Get(), 
		nullptr
	);
	UE_LOG(LogTemp, Warning, TEXT("[Rush]  Damage: %.1f"), ActualDamage);

	FVector KnockDirection = (HitCharacter->GetActorLocation() - OwnerBoss->GetActorLocation()).GetSafeNormal2D();
	if (KnockDirection.IsNearlyZero())
	{
		KnockDirection = RushDirection.IsNearlyZero() ? FVector::ForwardVector : RushDirection.GetSafeNormal2D();
	}

	FVector LaunchVelocity = KnockDirection * RushLaunchPower;
	LaunchVelocity.Z += RushLaunchUp;
	HitCharacter->LaunchCharacter(LaunchVelocity, true, true);
	
	UE_LOG(LogTemp, Warning, TEXT("[Rush] Launch: Power=%.1f, Up=%.1f"), RushLaunchPower, RushLaunchUp);

	if (UCPlayerKnockbackComponent* KnockbackComp = HitCharacter->FindComponentByClass<UCPlayerKnockbackComponent>())
	{
		KnockbackComp->StartKnockback(OwnerBoss.Get());
		UE_LOG(LogTemp, Warning, TEXT("[Rush] Knockback triggered"));
	}

	DamagedPlayers.Add(WeakHit);
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

void UCBossPattern_Rush::HandleMaxRushTime()
{
	if (State == ERushState::Rushing)
	{
		EndRushingInternal(ERushEndReason::MaxTime);
	}
}

void UCBossPattern_Rush::EndRushingInternal(ERushEndReason Reason, AActor* HitActor)
{
	if (State != ERushState::Rushing)
		return;

	// 종료 사유 로그
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

	if (UCharacterMovementComponent* Movement = OwnerBoss->GetCharacterMovement()) {
		Movement->StopMovementImmediately(); 
		Movement->MaxWalkSpeed = SavedMaxWalkSpeed;
		Movement->MaxAcceleration = SavedMaxAcceleration;
		Movement->BrakingDecelerationWalking = SavedBrakingDeceleration;
		Movement->GroundFriction = SavedGroundFriction;
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
		World->GetTimerManager().SetTimer(TH_Recovery, this, &UCBossPattern_Rush::HandlePatternComplete, CurrentRecoveryDuration, false);
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
	EnterState(ERushState::Cooldown);
	if (OwnerBoss.IsValid())
	{
		if (UCBossPatternManager* Manager = OwnerBoss->FindComponentByClass<UCBossPatternManager>())
		{
			Manager->NotifyCurrentPatternEnd(true);
		}
	}
}

void UCBossPattern_Rush::FaceTowards(const FVector& Direction, float DeltaSeconds)
{
	if (!HasValidOwner() || Direction.IsNearlyZero())
		return;
	
	FRotator CurrentRot = OwnerBoss->GetActorRotation();
	FRotator TargetRot = Direction.Rotation();
	FRotator NewRot = FMath::RInterpConstantTo(CurrentRot, TargetRot, DeltaSeconds, TurnRateDegPerSec);

	OwnerBoss->SetActorRotation(NewRot);
}

bool UCBossPattern_Rush::HasValidOwner() const
{
	return OwnerBoss.IsValid() && OwnerBoss->GetWorld() != nullptr;
}

float UCBossPattern_Rush::DistanceToTarget2D() const
{
	if (!HasValidOwner())
		return 0.f;
	
	return FVector::Dist2D(OwnerBoss->GetActorLocation(), RushTargetLocation);
}