#include "CBossPattern_Rush.h"
#include "00_Character/01_Enemy/CEnemyBossCharacter.h"
#include "00_Character/02_Component/01_EnemyComponent/CEnemyWeaponComponent.h"
#include "00_Character/02_Component/01_EnemyComponent/CEnemyBossPhaseComponent.h"
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
	PrimaryComponentTick.bCanEverTick = true;
}

void UCBossPattern_Rush::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogTemp, Log, TEXT("[Rush] BeginPlay called"));
}

void UCBossPattern_Rush::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UE_LOG(LogTemp, Log, TEXT("[Rush] EndPlay called"));
	Cleanup();
	Super::EndPlay(EndPlayReason);
}

void UCBossPattern_Rush::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	// Rushing 상태일 때만 이동 처리
	if (State == ERushState::Rushing)
	{
		TickRushMovement(DeltaTime);
	}
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

// State Management
void UCBossPattern_Rush::EnterState(ERushState NewState)
{
	if (State == NewState)
		return;
	
	ERushState PrevState = State;
	State = NewState;
	
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

// Telegraph Phase
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
			float TelegraphTime = FMath::Max(0.5f, CurrentPatternData.ExecutionTime * 0.2f);
			World->GetTimerManager().SetTimer(TH_Telegraph, this, &UCBossPattern_Rush::Anim_RushStart, TelegraphTime, false);
			UE_LOG(LogTemp, Log, TEXT("[Rush] Telegraph duration: %.2fs"), TelegraphTime);
		}
	}
}

// Rushing Phase
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
		
		UE_LOG(LogTemp, Log, TEXT("[Rush] Saved Movement Settings: Speed=%.1f, Accel=%.1f, Braking=%.1f"), 
			SavedMaxWalkSpeed, SavedMaxAcceleration, SavedBrakingDeceleration);
		
		Movement->SetMovementMode(MOVE_Walking);
		Movement->MaxWalkSpeed = RushSpeed;
		Movement->MaxAcceleration = 10000.0f;
		Movement->BrakingDecelerationWalking = 0.0f;
		Movement->GroundFriction = 0.0f;
		Movement->bOrientRotationToMovement = false;
		
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

	// Overlap 체크
	CheckOverlappingActors();
	if (State != ERushState::Rushing) return;

	// 충돌 감지
	PerformCollisionTrace();
	if (State != ERushState::Rushing) return;

	// 돌진 방향으로 이동 (고정된 방향 사용)
	if (UCharacterMovementComponent* Movement = OwnerBoss->GetCharacterMovement())
	{
		FVector TargetVelocity = LockedRushDirection * RushSpeed;
		Movement->Velocity = FMath::VInterpTo(Movement->Velocity, TargetVelocity, DeltaSeconds, 10.0f);
	}
}

void UCBossPattern_Rush::PerformCollisionTrace()
{
	if (!HasValidOwner()) return;

	FHitResult Hit;
	if (SweepAhead(Hit, CollisionTraceAhead))
	{
		AActor* HitActor = Hit.GetActor();
		if (!HitActor) return;

		if (Cast<ACharacter>(HitActor))
		{
			AController* HitController = Cast<ACharacter>(HitActor)->GetController();
			if (!Cast<AAIController>(HitController))
			{
				TWeakObjectPtr<AActor> WeakHit = HitActor;
				if (!DamagedPlayers.Contains(WeakHit))
				{
					UE_LOG(LogTemp, Warning, TEXT("[Rush] Sweep detected player ahead: %s"), *HitActor->GetName());
				}
			}
		}
	}
}

bool UCBossPattern_Rush::SweepAhead(FHitResult& OutHit, float Distance) const
{
	if (!HasValidOwner()) return false;

	const FVector Start = OwnerBoss->GetActorLocation();
	const FVector End = Start + LockedRushDirection * Distance;
	
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(OwnerBoss.Get());
	QueryParams.bTraceComplex = false;

	bool bHit = GetWorld()->SweepSingleByChannel(
		OutHit,
		Start,
		End,
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeSphere(CollisionRadius),
		QueryParams
	);

	if (bDrawDebug)
	{
		DrawDebugSphere(GetWorld(), Start, CollisionRadius, 12, bHit ? FColor::Red : FColor::Green, false, 0.1f);
		DrawDebugLine(GetWorld(), Start, End, bHit ? FColor::Red : FColor::Green, false, 0.1f, 0, 2.0f);
		
		if (bHit)
		{
			DrawDebugSphere(GetWorld(), OutHit.ImpactPoint, 30.0f, 8, FColor::Orange, false, 0.1f);
		}
	}
	
	if (bHit)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Rush] SweepAhead HIT: %s at distance %.1f"), 
			*GetNameSafe(OutHit.GetActor()), OutHit.Distance);
	}
	
	return bHit;
}

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

		float ActualDamage = UGameplayStatics::ApplyDamage
		(
			OtherActor, 
			RushDamage, 
			OwnerBoss->GetController(), 
			OwnerBoss.Get(), 
			nullptr
		);
		UE_LOG(LogTemp, Warning, TEXT("[Rush] OVERLAP Damage Applied: %.1f"), ActualDamage);

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

// Recovery Phase
void UCBossPattern_Rush::BeginRecoveryInternal(ERushEndReason Reason, AActor* HitActor)
{
	UE_LOG(LogTemp, Warning, TEXT("[Rush] BeginRecoveryInternal called"));
	
	EnterState(ERushState::Recovery);
	
	if (!HasValidOwner())
		return;

	if (UCharacterMovementComponent* Movement = OwnerBoss->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->Velocity = FVector::ZeroVector;
		
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
			AnimInstance->StopAllMontages(0.0f);
			
			if (RecoveryMontage)
			{
				float PlayRate = AnimInstance->Montage_Play(RecoveryMontage, 1.0f);
				UE_LOG(LogTemp, Warning, TEXT("[Rush] Playing Recovery Montage: %s (PlayRate: %.2f)"), 
					*RecoveryMontage->GetName(), PlayRate);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[Rush] RecoveryMontage is NULL! Cannot play recovery animation!"));
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

// Compatibility Methods
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

// Helper Functions
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
		UE_LOG(LogTemp, Warning, TEXT("[Rush] MISSED Player - Ending without cooldown"));
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