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

bool UCBossPattern_Rush::ExecutePattern(int32 PhaseIndex, const FBossPatternDefinition& PatternData)
{
	Super::ExecutePattern(PhaseIndex, PatternData);

	
	if (!HasValidOwner())
	{
		UE_LOG(LogTemp, Error, TEXT("[Rush] ExecutePattern REJECTED - Invalid Owner"));
		return false;  // ✅ 실패 반환
	}

	// ✅ Idle 상태가 아니면 실행 거부
	if (State != ERushState::Idle)
	{
		UE_LOG(LogTemp, Error, TEXT("[Rush] ExecutePattern REJECTED - Not in Idle state (Current: %d)"), (int32)State);
		return false;  // ✅ 실패 반환
	}

	// ✅ 쿨다운 중이면 실행 거부
	if (IsOnCooldown())
	{
		UE_LOG(LogTemp, Error, TEXT("[Rush] ExecutePattern REJECTED - Pattern is on cooldown"));
		return false;  // ✅ 실패 반환
	}

	
	UE_LOG(LogTemp, Warning, TEXT("[Rush] ========================================"));
	UE_LOG(LogTemp, Warning, TEXT("[Rush] Executing rush attack - Phase %d"), PhaseIndex);
	UE_LOG(LogTemp, Warning, TEXT("[Rush] ExecutionTime: %.2f, RecoveryTime: %.2f, Cooldown: %.2f"), 
		PatternData.ExecutionTime, PatternData.RecoveryTime, PatternData.Cooldown);
	UE_LOG(LogTemp, Warning, TEXT("[Rush] ========================================"));

	// PatternData 저장
	CurrentPatternData = PatternData;

	ResetTransientData();
	BeginTelegraphInternal();

	return true;
}

void UCBossPattern_Rush::OnPatternEnd()
{
	UE_LOG(LogTemp, Log, TEXT("[Rush] Pattern ended"));
	
	if (State == ERushState::Idle)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Rush] OnPatternEnd ignored - already Idle"));
		return;
	}
	
	UE_LOG(LogTemp, Log, TEXT("[Rush] OnPatternEnd called - Current State: %d"), (int32)State);

	if (State == ERushState::Recovery || State == ERushState::Cooldown)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Rush] OnPatternEnd ignored - Pattern is finishing normally (State: %d)"), (int32)State);
		return;
	}
	
	ClearTimers();
	
	if (State == ERushState::Rushing)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Rush] Force-stopped during Rushing - Forcing Recovery"));
		EndRushingInternal(ERushEndReason::Aborted, nullptr);
		return;
	}


	if (State == ERushState::Idle)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Rush] OnPatternEnd ignored - already Idle"));
		return;
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
	RushElapsedTime = 0.f;
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
	ClearTimers();
	
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
			// ✅ DataAsset의 TelegraphTime 사용
			float TelegraphTime = FMath::Max(0.1f, CurrentPatternData.TelegraphTime);
			World->GetTimerManager().SetTimer(TH_Telegraph, this, &UCBossPattern_Rush::Anim_RushStart, TelegraphTime, false);
			UE_LOG(LogTemp, Log, TEXT("[Rush] Telegraph duration: %.2fs (from DataAsset)"), TelegraphTime);
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

	RushElapsedTime = 0.f;
	
	// 🔥 방향 고정 체크 제거 - 이제 실시간 추적 사용
	AActor* Player = GetPlayerTarget();
	if (!Player)
	{
		UE_LOG(LogTemp, Error, TEXT("[Rush] No player target! Aborting rush."));
		HandlePatternComplete();
		return;
	}
	
	UE_LOG(LogTemp, Warning, TEXT("[Rush] Using REAL-TIME tracking"));

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
		// ✅ DataAsset의 ExecutionTime을 순수 Rush 시간으로 사용
		// (Telegraph는 별도 필드이므로 ExecutionTime과 무관)
		float ActualRushTime = CurrentPatternData.ExecutionTime;
		
		World->GetTimerManager().SetTimer(TH_MaxRush, this, &UCBossPattern_Rush::HandleMaxRushTime, ActualRushTime, false);
		UE_LOG(LogTemp, Warning, TEXT("[Rush] Rush duration timer set: %.2f seconds (ExecutionTime from DataAsset)"), 
			ActualRushTime);
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

	RushElapsedTime += DeltaSeconds;
	
	// ✅ 타이머가 HandleMaxRushTime을 호출하므로 여기서는 체크 불필요
	// (RushElapsedTime은 디버깅/로그용으로만 유지)
	
	// 플레이어 실시간 추적
	AActor* Player = GetPlayerTarget();
	if (!Player)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Rush] Player lost during rush"));
		EndRushingInternal(ERushEndReason::Aborted, nullptr);
		return;
	}

	// 현재 플레이어 방향 계산 (고정 방향 대신)
	FVector BossLocation = OwnerBoss->GetActorLocation();
	FVector PlayerLocation = Player->GetActorLocation();
	FVector ToPlayer = (PlayerLocation - BossLocation).GetSafeNormal2D();

	// 부드러운 회전
	if (!ToPlayer.IsNearlyZero())
	{
		FRotator CurrentRotation = OwnerBoss->GetActorRotation();
		FRotator TargetRotation = ToPlayer.Rotation();
		FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaSeconds, TurnRateDegPerSec);
		OwnerBoss->SetActorRotation(NewRotation);
	}

	// 직접 속도 설정 (VInterpTo 제거)
	if (UCharacterMovementComponent* Movement = OwnerBoss->GetCharacterMovement())
	{
		FVector TargetVelocity = ToPlayer * RushSpeed;
		Movement->Velocity = TargetVelocity; // 직접 설정
		Movement->UpdateComponentVelocity();
	}

	// 로그 (0.5초마다)
	static float LogTimer = 0.f;
	LogTimer += DeltaSeconds;
	if (LogTimer >= 0.5f)
	{
		LogTimer = 0.f;
		FVector CurrentVelocity = OwnerBoss->GetVelocity();
		UE_LOG(LogTemp, Log, TEXT("[Rush] Rushing... Velocity: %.1f, ToPlayer: %s"), 
			CurrentVelocity.Size(), *ToPlayer.ToString());
	}

	// Overlap 체크
	CheckOverlappingActors();
	if (State != ERushState::Rushing) return;

	// 충돌 감지
	PerformCollisionTrace();
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
	// 🔥 고정 방향 대신 현재 forward vector 사용
	const FVector ForwardDir = OwnerBoss->GetActorForwardVector();
	const FVector End = Start + ForwardDir * Distance;
	
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(OwnerBoss.Get());
	QueryParams.bTraceComplex = false;

	// 🔥 투사체 무시 (BP_BossProjectile 등)
	TArray<AActor*> AllActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), AllActors);
	for (AActor* Actor : AllActors)
	{
		if (Actor && (Actor->GetName().Contains(TEXT("Projectile")) || 
		              Actor->GetName().Contains(TEXT("Coconut"))))
		{
			QueryParams.AddIgnoredActor(Actor);
		}
	}

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
	if (State != ERushState::Rushing)
	{
		return;
	}
	
	UE_LOG(LogTemp, Error, TEXT("[Rush] ========== SAFETY TIMEOUT TRIGGERED =========="));
	UE_LOG(LogTemp, Error, TEXT("[Rush] Current State: %d"), (int32)State);
	UE_LOG(LogTemp, Error, TEXT("[Rush] Elapsed Time: %.2f"), RushElapsedTime);
	
	EndRushingInternal(ERushEndReason::MaxTime, nullptr);
	if (State != ERushState::Recovery && State != ERushState::Idle)
	{
		UE_LOG(LogTemp, Error, TEXT("[Rush] FORCING RECOVERY STATE!"));
		BeginRecoveryInternal(ERushEndReason::MaxTime, nullptr);
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
	
	if (State == ERushState::Recovery)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Rush] Already in Recovery state - ignoring duplicate call"));
		return;
	}

	if (UWorld* World = OwnerBoss->GetWorld())
	{
		World->GetTimerManager().ClearTimer(TH_Recovery);
	}
	
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
		World->GetTimerManager().ClearTimer(TH_Recovery); //기존 리커버리 타이머 클리어
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

	if (State != ERushState::Recovery)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Rush] HandlePatternComplete ignored - not in Recovery (Current: %d)"), (int32)State);
		return;
	}

	EnterState(ERushState::Cooldown);
	
	// ✅ 쿨다운 적용 여부 결정
	bool bApplyCooldown = true;
	
	if (LastEndReason == ERushEndReason::Aborted)
	{
		// 중단된 경우: 쿨다운 없음 (페널티 없이 재시도 가능)
		UE_LOG(LogTemp, Warning, TEXT("[Rush] ABORTED - No cooldown"));
		bApplyCooldown = false;
	}
	else if (LastEndReason == ERushEndReason::HitPlayer)
	{
		// 플레이어 히트: 쿨다운 적용
		UE_LOG(LogTemp, Warning, TEXT("[Rush] HIT Player - Cooldown applied"));
		bApplyCooldown = true;
	}
	else if (LastEndReason == ERushEndReason::MaxTime)
	{
		// 시간 초과 (미스): 쿨다운 적용 (무한 스팸 방지)
		UE_LOG(LogTemp, Warning, TEXT("[Rush] MISSED Player (MaxTime) - Cooldown applied"));
		bApplyCooldown = true;
	}
	else
	{
		// 기타 (ReachedTarget 등): 쿨다운 적용
		UE_LOG(LogTemp, Warning, TEXT("[Rush] Ended (Reason: %d) - Cooldown applied"), (int32)LastEndReason);
		bApplyCooldown = true;
	}
	
	FinishPattern(bApplyCooldown);
	EnterState(ERushState::Idle);
	UE_LOG(LogTemp, Warning, TEXT("[Rush] Pattern Complete - Returned to Idle state"));

}

bool UCBossPattern_Rush::HasValidOwner() const
{
	return OwnerBoss.IsValid() && OwnerBoss->GetWorld() != nullptr;
}