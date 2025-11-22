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
		return false; 
	}

	
	if (State != ERushState::Idle)
	{
		UE_LOG(LogTemp, Error, TEXT("[Rush] ExecutePattern REJECTED - Not in Idle state (Current: %d)"), (int32)State);
		return false;  
	}

	if (IsOnCooldown())
	{
		UE_LOG(LogTemp, Error, TEXT("[Rush] ExecutePattern REJECTED - Pattern is on cooldown"));
		return false;  
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
	
	if (State == ERushState::Recovery || State == ERushState::Cooldown || State == ERushState::Idle)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Rush] OnPatternEnd ignored - Pattern is already finishing or finished."));
		return;
	}
	
	if (State == ERushState::Rushing || State == ERushState::Telegraph)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Rush] Force-stopped during Rushing - Forcing Recovery"));
		EndRushingInternal(ERushEndReason::Aborted, nullptr);
		return;
	}
	
	UE_LOG(LogTemp, Log, TEXT("[Rush] OnPatternEnd: Cleaning up from state %s"), *UEnum::GetValueAsString(State));
	ClearTimers();
	if (OwnerBoss.IsValid())
	{
		OwnerBoss->SetIsBossRushing(false);
	}

	EnterState(ERushState::Idle);
	FinishPattern(false);
	Super::OnPatternEnd();
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
    
	// [변경 1] 채널을 ECC_Pawn -> ECC_WorldDynamic으로 변경
	// 이유: 리스폰 후 플레이어가 Pawn 채널은 Overlap하지만, WorldDynamic은 Block하므로 확실히 감지됨
	bool bHit = GetWorld()->SweepSingleByChannel(
		Hit,
		OwnerBoss->GetActorLocation(),
		OwnerBoss->GetActorLocation() + OwnerBoss->GetActorForwardVector() * CollisionTraceAhead,
		FQuat::Identity,
		ECC_WorldDynamic,  // <--- 여기를 변경
		FCollisionShape::MakeSphere(CollisionRadius),
		FCollisionQueryParams(SCENE_QUERY_STAT(RushTrace), false, OwnerBoss.Get())
	);

	if (bHit)
	{
		AActor* HitActor = Hit.GetActor();
		// 플레이어(Character)이면서 AI가 아닌 경우만 처리
		if (ACharacter* HitChar = Cast<ACharacter>(HitActor))
		{
			if (!Cast<AAIController>(HitChar->GetController()))
			{
				// [변경 2] 감지 시 데미지 함수 호출 (기존에는 로그만 있었음)
				UE_LOG(LogTemp, Warning, TEXT("[Rush] Sweep Hit Player via WorldDynamic!"));
				ProcessPlayerHit(HitActor);
			}
		}
	}
}

bool UCBossPattern_Rush::SweepAhead(FHitResult& OutHit, float Distance) const
{
	if (!HasValidOwner()) return false;

	const FVector Start = OwnerBoss->GetActorLocation();
	const FVector ForwardDir = OwnerBoss->GetActorForwardVector();
	const FVector End = Start + ForwardDir * Distance;
	
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(OwnerBoss.Get());
	QueryParams.bTraceComplex = false;

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
		if (OtherActor && OtherActor != OwnerBoss.Get())
		{
			if (ACharacter* HitChar = Cast<ACharacter>(OtherActor))
			{
				// 플레이어 확인
				if (!Cast<AAIController>(HitChar->GetController()))
				{
					ProcessPlayerHit(OtherActor);
					return; 
				}
			}
		}
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

void UCBossPattern_Rush::ProcessPlayerHit(AActor* TargetActor)
{
	if (!HasValidOwner() || !TargetActor) return;

	TWeakObjectPtr<AActor> WeakTarget = TargetActor;
	if (DamagedPlayers.Contains(WeakTarget)) return;

	// 데미지 적용
	UGameplayStatics::ApplyDamage(TargetActor, RushDamage, OwnerBoss->GetController(), OwnerBoss.Get(), nullptr);

	// 넉백 적용
	ACharacter* HitCharacter = Cast<ACharacter>(TargetActor);
	if (HitCharacter)
	{
		FVector KnockDirection = LockedRushDirection.IsNearlyZero() ? OwnerBoss->GetActorForwardVector() : LockedRushDirection;
		FVector LaunchVelocity = KnockDirection * RushLaunchPower;
		LaunchVelocity.Z += RushLaunchUp;
		HitCharacter->LaunchCharacter(LaunchVelocity, true, true);

		if (UCPlayerKnockbackComponent* KnockbackComp = HitCharacter->FindComponentByClass<UCPlayerKnockbackComponent>())
		{
			KnockbackComp->StartKnockback(OwnerBoss.Get());
		}
	}

	DamagedPlayers.Add(WeakTarget);
	EndRushingInternal(ERushEndReason::HitPlayer, TargetActor);
}

// Helper Functions
void UCBossPattern_Rush::HandlePatternComplete()
{
	if (State != ERushState::Recovery)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Rush] HandlePatternComplete ignored - Not in Recovery state (Current: %s)"), *UEnum::GetValueAsString(State));
		return;
	}
	
	
	if (OwnerBoss.IsValid() && OwnerBoss->GetWorld())
	{
		OwnerBoss->GetWorld()->GetTimerManager().ClearTimer(TH_Recovery);
	}
	
	if (PhaseComponent.IsValid())
	{
		// 이미 다른 패턴이 실행 중이면 조용히 종료
		if (PhaseComponent->GetActivePatternId() != PatternId && 
			PhaseComponent->GetActivePatternId() != NAME_None)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Rush] HandlePatternComplete - Another pattern already started (%s), cleaning up silently"), 
				*PhaseComponent->GetActivePatternId().ToString());
            
			EnterState(ERushState::Idle);
			return;  // FinishPattern 호출하지 않음
		}
	}
	

	EnterState(ERushState::Cooldown);
	
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