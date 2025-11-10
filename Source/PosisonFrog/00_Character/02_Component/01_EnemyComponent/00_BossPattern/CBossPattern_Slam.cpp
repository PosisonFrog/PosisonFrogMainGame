#include "CBossPattern_Slam.h"
#include "00_Character/01_Enemy/CEnemyBossCharacter.h"
#include "00_Character/02_Component/01_EnemyComponent/CEnemyWeaponComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"

UCBossPattern_Slam::UCBossPattern_Slam()
{
	PatternId = FName("Slam");
	
	CurrentWarnDuration = Phase1_WarnDuration;
	CurrentRecoveryDuration = Phase1_RecoveryDuration;
}

void UCBossPattern_Slam::BeginDestroy()
{
	UE_LOG(LogTemp, Log, TEXT("[Slam] BeginDestroy called"));
	Cleanup();
	Super::BeginDestroy();
}

void UCBossPattern_Slam::ExecutePattern(int32 PhaseIndex)
{
	Super::ExecutePattern(PhaseIndex);

	UE_LOG(LogTemp, Warning, TEXT("[Slam] Executing slam attack - Phase %d"), PhaseIndex);
	UE_LOG(LogTemp, Log, TEXT("[Slam] Warn: %.2fs, Recovery: %.2fs"), 
		   CurrentWarnDuration, CurrentRecoveryDuration);

	if (!OwnerBoss.IsValid())  
	{
		UE_LOG(LogTemp, Error, TEXT("[Slam] Invalid OwnerBoss"));
		return;
	}

	
	if (SlamMontage)
	{
		PlayMontage(SlamMontage);
		FTimerHandle TempTimer;
		GetWorld()->GetTimerManager().SetTimer(TempTimer, this, &UCBossPattern_Slam::PlayImpactEffectsAndDamage, 1.0f, false);
	}
	else
	{
		PlayImpactEffectsAndDamage();
	}
}

void UCBossPattern_Slam::OnPatternEnd()
{
	Super::OnPatternEnd();

	UE_LOG(LogTemp, Log, TEXT("[Slam] Pattern ended"));
}

void UCBossPattern_Slam::Cleanup()
{
	Super::Cleanup();
	// 필요한 경우 타이머 정리 등 추가
}

void UCBossPattern_Slam::UpdatePhaseSettings(int32 PhaseIndex)
{
	Super::UpdatePhaseSettings(PhaseIndex);

	if (PhaseIndex == 0)
	{
		CurrentWarnDuration = Phase1_WarnDuration;
		CurrentRecoveryDuration = Phase1_RecoveryDuration;
		UE_LOG(LogTemp, Log, TEXT("[Slam] Updated to Phase 1 settings"));
	}
	else if (PhaseIndex >= 1)
	{
		CurrentWarnDuration = Phase2_WarnDuration;
		CurrentRecoveryDuration = Phase2_RecoveryDuration;
		UE_LOG(LogTemp, Log, TEXT("[Slam] Updated to Phase 2 settings"));
	}
}


void UCBossPattern_Slam::PlayImpactEffectsAndDamage()
{
	if (!OwnerBoss.IsValid()) return;
	UWorld* World = OwnerBoss->GetWorld();
	if (!World) return;

	const FVector ImpactLocation = OwnerBoss->GetActorLocation();

	UGameplayStatics::ApplyRadialDamage(
		World,
		SlamDamage,
		ImpactLocation,
		DamageRadius,
		nullptr, // DamageType Class
		TArray<AActor*>(), // 무시할 액터
		OwnerBoss.Get(),
		OwnerBoss->GetController(),
		true // bDoFullDamage
	);
	UE_LOG(LogTemp, Warning, TEXT("[Slam] Applied radial damage at %s"), *ImpactLocation.ToString());

	if (GroundImpactEffect)
	{
		UGameplayStatics::SpawnEmitterAtLocation(
			World,
			GroundImpactEffect,
			ImpactLocation,
			FRotator::ZeroRotator
		);
	}

	if (GroundImpactSound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			World,
			GroundImpactSound,
			ImpactLocation
		);
	}

	if (GroundImpactShake)
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
		{
			PC->ClientStartCameraShake(GroundImpactShake);
		}
	}
}

