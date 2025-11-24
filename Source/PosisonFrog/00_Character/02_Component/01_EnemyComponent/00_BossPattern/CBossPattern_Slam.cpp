#include "CBossPattern_Slam.h"
#include "00_Character/01_Enemy/CEnemyBossCharacter.h"
#include "00_Character/01_Enemy/01_AIController/BossAIController.h"
#include "00_Character/02_Component/01_EnemyComponent/CEnemyWeaponComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"

UCBossPattern_Slam::UCBossPattern_Slam()
{
	PatternId = FName("Slam");
	PrimaryComponentTick.bCanEverTick = false;
}

bool UCBossPattern_Slam::ExecutePattern(int32 PhaseIndex, const FBossPatternDefinition& PatternData)
{
	Super::ExecutePattern(PhaseIndex, PatternData);

	if (!OwnerBoss.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[Slam] ExecutePattern REJECTED - Invalid OwnerBoss"));
		return false;
	}

	if (IsOnCooldown())
	{
		UE_LOG(LogTemp, Error, TEXT("[Slam] ExecutePattern REJECTED - Pattern is on cooldown"));
		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Slam] Executing slam attack - Phase %d"), PhaseIndex);
	UE_LOG(LogTemp, Log, TEXT("[Slam] ExecutionTime: %.2f, RecoveryTime: %.2f"), 
		PatternData.ExecutionTime, PatternData.RecoveryTime);

	CurrentPatternData = PatternData;

	ClearTimers();

	if (AAIController* AI = GetBossAI())
	{
		if (ABossAIController* BossAI = Cast<ABossAIController>(AI))
		{
			BossAI->SetChaseEnabled(false); // 추적 끄기
			BossAI->StopMovement();         // 이동 멈추기
		}
	}
	LockedImpactLocation = OwnerBoss->GetActorLocation() + (OwnerBoss->GetActorForwardVector() * 530.0f);
	if (UCharacterMovementComponent* MoveComp = OwnerBoss->GetCharacterMovement())
	{
		bSavedOrientRotation = MoveComp->bOrientRotationToMovement;
		bSavedUseControllerDesiredRotation = MoveComp->bUseControllerDesiredRotation;
		
		MoveComp->bOrientRotationToMovement = false; 
		MoveComp->bUseControllerDesiredRotation = false; 
        
		MoveComp->StopMovementImmediately();
	}

	float Duration = 0.0f;
	if (SlamMontage)
	{
		Duration = PlayMontage(SlamMontage);
		if (Duration <= 0.f) 
		{
			Duration = PatternData.ExecutionTime > 0.f ? PatternData.ExecutionTime : 1.5f;
		}

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				TH_ImpactEffect, 
				this, 
				&UCBossPattern_Slam::PlayImpactEffectsAndDamage, 
				Duration * 0.52f, //몽타주 재생 진행률 49퍼에서 데미지 입히기
				false
			);
		}
	}
	else
	{
		PlayImpactEffectsAndDamage();
		Duration = PatternData.ExecutionTime > 0.f ? PatternData.ExecutionTime : 1.0f;
	}

	// DataAsset의 RecoveryTime 사용
	float FinishTime = Duration + CurrentPatternData.RecoveryTime;
	
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			TH_Finish, 
			this, 
			&UCBossPattern_Slam::FinishSlam, 
			FinishTime, 
			false
		);
		
		UE_LOG(LogTemp, Log, TEXT("[Slam] Pattern will finish in %.2f seconds (Execution: %.2f + Recovery: %.2f)"), 
			FinishTime, Duration, CurrentPatternData.RecoveryTime);
	}
	
	return true;  
}

void UCBossPattern_Slam::OnPatternEnd()
{
	if (AAIController* AI = GetBossAI())
	{
		if (ABossAIController* BossAI = Cast<ABossAIController>(AI))
		{
			BossAI->SetChaseEnabled(true);
		}
	}
	
	if (OwnerBoss.IsValid())
	{
		if (UCharacterMovementComponent* MoveComp = OwnerBoss->GetCharacterMovement())
		{
			MoveComp->bOrientRotationToMovement = bSavedOrientRotation;
			MoveComp->bUseControllerDesiredRotation = bSavedUseControllerDesiredRotation;
		}
	}
	
	Super::OnPatternEnd();
	ClearTimers();
	
	UE_LOG(LogTemp, Log, TEXT("[Slam] Pattern ended"));
}

void UCBossPattern_Slam::Cleanup()
{
	if (OwnerBoss.IsValid())
	{
		if (UCharacterMovementComponent* MoveComp = OwnerBoss->GetCharacterMovement())
		{
			MoveComp->bOrientRotationToMovement = true; // 강제 복구
		}
	}
	
	Super::Cleanup();
	ClearTimers();
}

void UCBossPattern_Slam::ClearTimers()
{
	if (UWorld* World = GetWorld())
	{
		FTimerManager& TM = World->GetTimerManager();
		TM.ClearTimer(TH_Finish);
		TM.ClearTimer(TH_ImpactEffect);
	}
}

void UCBossPattern_Slam::FinishSlam()
{
	FinishPattern(true);
}

void UCBossPattern_Slam::PlayImpactEffectsAndDamage()
{
	if (!OwnerBoss.IsValid()) return;
	UWorld* World = OwnerBoss->GetWorld();
	if (!World) return;
	
	// 데미지 적용
	UGameplayStatics::ApplyRadialDamage(
		World,
		SlamDamage,
		LockedImpactLocation,
		DamageRadius,
		nullptr, 
		TArray<AActor*>(), 
		OwnerBoss.Get(),
		OwnerBoss->GetController(),
		true 
	);
	UE_LOG(LogTemp, Warning, TEXT("[Slam] Applied radial damage at %s"), *LockedImpactLocation.ToString());

	// 이펙트
	if (GroundImpactEffect)
	{
		UGameplayStatics::SpawnEmitterAtLocation(
			World,
			GroundImpactEffect,
			LockedImpactLocation,
			FRotator::ZeroRotator
		);
	}

	// 사운드
	if (GroundImpactSound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			World,
			GroundImpactSound,
			LockedImpactLocation
		);
	}

	// 카메라 쉐이크
	if (GroundImpactShake)
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
		{
			PC->ClientStartCameraShake(GroundImpactShake);
		}
	}

	if (OwnerBoss.IsValid())
	{
		if (ABossAIController* BossAI = Cast<ABossAIController>(GetBossAI()))
		{
			// AI 추적을 켜서 회전 계산을 시작하게 함 (이동은 MoveTo가 호출되어야 하므로 제자리 회전만 함)
			BossAI->SetChaseEnabled(true); 
		}

		if (UCharacterMovementComponent* MoveComp = OwnerBoss->GetCharacterMovement())
		{
			// 컨트롤러 방향(AI가 바라보는 곳)으로 몸을 돌리도록 설정 복구
			MoveComp->bUseControllerDesiredRotation = true;
		}
	}
}