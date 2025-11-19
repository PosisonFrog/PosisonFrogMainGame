#include "BossAIController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"

ABossAIController::ABossAIController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ABossAIController::SetTargetPlayer(AActor* NewTarget)
{
	if (!IsValid(NewTarget))
	{
		if (bIsMovingToTarget)
		{
			StopMovement();
			bIsMovingToTarget = false;
		}
			
		if (TargetPlayer)
		{
			UE_LOG(LogTemp, Warning, TEXT("[BossAI] Target cleared"));
		}
			
		TargetPlayer = nullptr;
		return;
	}
	
	TargetPlayer = NewTarget;
	UE_LOG(LogTemp, Warning, TEXT("[BossAI] Target set to: %s"), *GetNameSafe(NewTarget));
}

void ABossAIController::SetChaseEnabled(bool bEnabled)
{
	UE_LOG(LogTemp, Error, TEXT("[BossAI]  SetChaseEnabled(%s)"), bEnabled ? TEXT("TRUE") : TEXT("FALSE"));
	bChaseEnabled = bEnabled;
	
	if (!bEnabled && bIsMovingToTarget)
	{
		StopMovement();
		bIsMovingToTarget = false;
		UE_LOG(LogTemp, Log, TEXT("[BossAI] Chase disabled - Stopped movement"));
	}
	else if (bEnabled)
	{
		UE_LOG(LogTemp, Log, TEXT("[BossAI] Chase enabled"));
	}
}

void ABossAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
    
	TargetPlayer = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!IsValid(TargetPlayer))
	{
		TargetPlayer = nullptr;
	}
	
	UE_LOG(LogTemp, Error, TEXT("[BossAI] ========================================"));
	UE_LOG(LogTemp, Error, TEXT("[BossAI] Possessed %s, Target: %s"),
		*GetNameSafe(InPawn), *GetNameSafe(TargetPlayer));
	UE_LOG(LogTemp, Error, TEXT("[BossAI] Initial bChaseEnabled = %s"), bChaseEnabled ? TEXT("TRUE") : TEXT("FALSE"));
	UE_LOG(LogTemp, Error, TEXT("[BossAI] ========================================"));
}

void ABossAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	if (!EnsureValidTarget())
	{
		return;
	}
	
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		UE_LOG(LogTemp, Error, TEXT("[BossAI] No controlled pawn!"));
		return;
	}

	if (!bChaseEnabled)
	{
		return;
	}

	FVector Direction = TargetPlayer->GetActorLocation() - ControlledPawn->GetActorLocation();
	Direction.Z = 0.f;
	
	if (!Direction.IsNearlyZero())
	{
		FRotator TargetRotation = Direction.Rotation();
		FRotator CurrentRotation = GetControlRotation();
		
		FRotator NewRotation = FMath::RInterpTo(
			CurrentRotation, 
			TargetRotation, 
			DeltaTime, 
			8.0f
		);
		
		SetControlRotation(NewRotation);
	}
	
	TimeSinceMoveUpdate += DeltaTime;
	
	if (TimeSinceMoveUpdate >= MoveUpdateInterval)
	{
		TimeSinceMoveUpdate = 0.f;
		
		float DistanceToTarget = FVector::Dist(ControlledPawn->GetActorLocation(), TargetPlayer->GetActorLocation());
		
		UE_LOG(LogTemp, Warning, TEXT("[BossAI] Distance to player: %.1f (Stop: %.1f, Chase: %.1f)"), 
			   DistanceToTarget, StopDistance, ChaseDistance);
		
		if (DistanceToTarget <= StopDistance)
		{
			if (bIsMovingToTarget)
			{
				StopMovement();
				bIsMovingToTarget = false;
				UE_LOG(LogTemp, Warning, TEXT("[BossAI] Stopped - Close enough (%.1f)"), DistanceToTarget);
			}
		}
		else if (DistanceToTarget > ChaseDistance)
		{
			// MoveTo 실행
			EPathFollowingRequestResult::Type MoveResult = MoveToActor(
				TargetPlayer,
				StopDistance,
				true,  // bStopOnOverlap
				true,  // bUsePathfinding
				false, // bCanStrafe
				nullptr,
				true   // bAllowPartialPath
			);
			
			if (MoveResult == EPathFollowingRequestResult::RequestSuccessful)
			{
				bIsMovingToTarget = true;
				UE_LOG(LogTemp, Warning, TEXT("[BossAI] CHASING player! (Distance: %.1f)"), DistanceToTarget);
			}
			else if (MoveResult == EPathFollowingRequestResult::Failed)
			{
				UE_LOG(LogTemp, Error, TEXT("[BossAI] MoveTo FAILED! Distance: %.1f (Check NavMesh!)"), DistanceToTarget);
			}
			else if (MoveResult == EPathFollowingRequestResult::AlreadyAtGoal)
			{
				UE_LOG(LogTemp, Log, TEXT("[BossAI] Already at goal"));
			}
		}
		// 적당한 거리 (유지)
		else
		{
			UE_LOG(LogTemp, Log, TEXT("[BossAI] In range (%.1f), maintaining..."), DistanceToTarget);
		}
	}
}

bool ABossAIController::EnsureValidTarget()
{
	if (IsValid(TargetPlayer))
	{
		return true;
	}
	
	if (bIsMovingToTarget)
	{
		StopMovement();
		bIsMovingToTarget = false;
	}
	
	TargetPlayer = nullptr;
	
	if (UWorld* World = GetWorld())
	{
		APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0);
		if (IsValid(PlayerPawn))
		{
			TargetPlayer = PlayerPawn;
			UE_LOG(LogTemp, Warning, TEXT("[BossAI] Target reacquired: %s"), *GetNameSafe(TargetPlayer));
			return true;
		}
	}
	
	return false;
}