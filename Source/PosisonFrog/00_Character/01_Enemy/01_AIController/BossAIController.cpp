#include "BossAIController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

ABossAIController::ABossAIController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ABossAIController::SetTargetPlayer(AActor* NewTarget)
{
	TargetPlayer = NewTarget;
	UE_LOG(LogTemp, Warning, TEXT("[BossAI] Target set to: %s"), *GetNameSafe(NewTarget));
}

void ABossAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
    
	// 플레이어 찾기
	TargetPlayer = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    
	UE_LOG(LogTemp, Log, TEXT("[BossAI] Possessed %s, Target: %s"), 
		   *GetNameSafe(InPawn), *GetNameSafe(TargetPlayer));
}

void ABossAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	if (!TargetPlayer)
	{
		TargetPlayer = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
		if (TargetPlayer)
		{
			UE_LOG(LogTemp, Warning, TEXT("[BossAI] Target found: %s"), *GetNameSafe(TargetPlayer));
		}
		return; // 아직 못 찾았으면 회전 스킵
	}
	
	TimeSinceLastUpdate += DeltaTime;
    
	// 주기적으로 플레이어를 바라보기
	if (TimeSinceLastUpdate >= UpdateInterval)
	{
		TimeSinceLastUpdate = 0.f;
        
		if (TargetPlayer && GetPawn())
		{
			// 플레이어 방향으로 회전
			FVector Direction = TargetPlayer->GetActorLocation() - GetPawn()->GetActorLocation();
			Direction.Z = 0.f;
            
			if (!Direction.IsNearlyZero())
			{
				FRotator NewRotation = Direction.Rotation();
				SetControlRotation(NewRotation);
			}
		}
	}
}