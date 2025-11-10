#include "CBossPattern_Rush.h"
#include "00_Character/01_Enemy/CEnemyBossCharacter.h"
#include "00_Character/02_Component/01_EnemyComponent/CEnemyWeaponComponent.h"
#include "00_Character/01_Enemy/01_AIController/BossAIController.h"
#include "AIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h" 
#include "Kismet/KismetSystemLibrary.h"

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
	DamagedPlayers.Empty();
	
	UE_LOG(LogTemp, Warning, TEXT("[Rush] Executing rush attack - Phase %d"), PhaseIndex);
	UE_LOG(LogTemp, Log, TEXT("[Rush] Telegraph: %.2fs, Recovery: %.2fs"), 
		   CurrentTelegraphDuration, CurrentRecoveryDuration);

	if (!OwnerBoss.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[Rush] Invalid OwnerBoss"));
		return;
	}

	// Chase 비활성화
	if (ABossAIController* BossAI = Cast<ABossAIController>(GetBossAI()))
	{
		BossAI->SetChaseEnabled(false);
		UE_LOG(LogTemp, Log, TEXT("[Rush] Disabled chase for Rush"));
	}

	// 플레이어 위치 저장
	if (AActor* Player = GetPlayerTarget())
	{
		RushTargetLocation = Player->GetActorLocation();
		
		const FVector Direction = (RushTargetLocation - OwnerBoss->GetActorLocation()).GetSafeNormal2D();
		if (!Direction.IsNearlyZero())
		{
			OwnerBoss->SetActorRotation(Direction.Rotation());
		}

		UE_LOG(LogTemp, Log, TEXT("[Rush] Target location set: %s"), *RushTargetLocation.ToString());
	}

	// 경고 시간 후 돌진 시작
	if (UWorld* World = OwnerBoss->GetWorld())
	{
		World->GetTimerManager().SetTimer(
			RushDelayTimer,
			this,
			&UCBossPattern_Rush::StartRush,
			CurrentTelegraphDuration,
			false
		);
	}
}

void UCBossPattern_Rush::OnPatternEnd()
{
	Super::OnPatternEnd();
	
	if (OwnerBoss.IsValid())
	{
		OwnerBoss->SetIsBossRushing(false);
	}
	bIsRushing = false;
	
	// Chase 재활성화
	if (ABossAIController* BossAI = Cast<ABossAIController>(GetBossAI()))
	{
		BossAI->SetChaseEnabled(true);
		UE_LOG(LogTemp, Log, TEXT("[Rush] Re-enabled chase after Rush"));
	}

	// 이동 속도 원복
	if (OwnerBoss.IsValid() && OwnerBoss->GetCharacterMovement())
	{
		OwnerBoss->GetCharacterMovement()->MaxWalkSpeed = 400.0f;
	}

	UE_LOG(LogTemp, Log, TEXT("[Rush] Pattern ended"));
}

void UCBossPattern_Rush::Cleanup()
{
	Super::Cleanup();

	if (!OwnerBoss.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Rush] OwnerBoss is invalid during cleanup"));
		return;
	}

	UWorld* World = OwnerBoss->GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Rush] World is invalid during cleanup"));
		return;
	}

	FTimerManager& TimerManager = World->GetTimerManager();
	TimerManager.ClearTimer(RushDelayTimer);
	TimerManager.ClearTimer(RushMoveTimer);

	bIsRushing = false;
}

void UCBossPattern_Rush::UpdatePhaseSettings(int32 PhaseIndex)
{
	Super::UpdatePhaseSettings(PhaseIndex);

	if (PhaseIndex == 0)
	{
		CurrentTelegraphDuration = Phase1_TelegraphDuration;
		CurrentRecoveryDuration = Phase1_RecoveryDuration;
		UE_LOG(LogTemp, Log, TEXT("[Rush] Updated to Phase 1 settings"));
	}
	else if (PhaseIndex >= 1)
	{
		CurrentTelegraphDuration = Phase2_TelegraphDuration;
		CurrentRecoveryDuration = Phase2_RecoveryDuration;
		UE_LOG(LogTemp, Log, TEXT("[Rush] Updated to Phase 2 settings"));
	}
}

void UCBossPattern_Rush::TickRushMovement(float DeltaTime)
{
	if (!bIsRushing || !OwnerBoss.IsValid())
	{
		return;
	}
	
	FVector Start = OwnerBoss->GetActorLocation();
	FVector End = Start + OwnerBoss->GetActorForwardVector() * 100.f;
	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(OwnerBoss.Get());
	FHitResult HitResult;

	// 플레이어만 감지하도록 설정
	bool bHit = UKismetSystemLibrary::SphereTraceSingle(
		OwnerBoss->GetWorld(),
		Start,
		End,
		100.0f, // 충돌 감지 반경
		UEngineTypes::ConvertToTraceType(ECC_Pawn),
		false,
		ActorsToIgnore,
		EDrawDebugTrace::None,
		HitResult,
		true
	);

	if (bHit)
	{
		AActor* HitActor = HitResult.GetActor();
		if (HitActor && HitActor->IsA<ACharacter>() && HitActor != OwnerBoss.Get() && !DamagedPlayers.Contains(HitActor))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Rush] Hit Player: %s"), *HitActor->GetName());
            
			//데미지 적용
			UGameplayStatics::ApplyDamage(HitActor, RushDamage, OwnerBoss->GetController(), OwnerBoss.Get(), nullptr);
            
			// 넉백
			ACharacter* HitCharacter = Cast<ACharacter>(HitActor);
			if (HitCharacter)
			{
				FVector LaunchDirection = (HitCharacter->GetActorLocation() - OwnerBoss->GetActorLocation()).GetSafeNormal();
				LaunchDirection.Z = 0.5f; // 약간 위로 띄우기
				HitCharacter->LaunchCharacter(LaunchDirection.GetSafeNormal() * RushLaunchPower, true, true);
			}

			DamagedPlayers.Add(HitActor); // 중복 데미지 방지
		}
	}
	
	const FVector CurrentLocation = OwnerBoss->GetActorLocation();
	const FVector Direction = (RushTargetLocation - CurrentLocation).GetSafeNormal2D();

	const float Distance = FVector::Dist2D(CurrentLocation, RushTargetLocation);
	if (Distance <= RushAcceptanceRadius)
	{
		bIsRushing = false;
		UE_LOG(LogTemp, Log, TEXT("[Rush] Reached target location"));
		return;
	}

	OwnerBoss->AddMovementInput(Direction, 1.0f);
}

void UCBossPattern_Rush::HandleRushMovementStart()
{
	bIsRushing = true;
	UE_LOG(LogTemp, Log, TEXT("[Rush] Movement started"));
}

void UCBossPattern_Rush::HandleRushMovementStop()
{
	bIsRushing = false;
	UE_LOG(LogTemp, Log, TEXT("[Rush] Movement stopped"));
}

void UCBossPattern_Rush::StartRush()
{
	if (!OwnerBoss.IsValid())
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Rush] Starting rush movement!"));

	if (UCharacterMovementComponent* Movement = OwnerBoss->GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = RushSpeed;
	}
	
	OwnerBoss->SetIsBossRushing(true);
	bIsRushing = true;
	
}

