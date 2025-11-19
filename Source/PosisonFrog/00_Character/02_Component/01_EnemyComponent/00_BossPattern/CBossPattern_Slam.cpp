#include "CBossPattern_Slam.h"
#include "00_Character/01_Enemy/CEnemyBossCharacter.h"
#include "00_Character/02_Component/01_EnemyComponent/CEnemyWeaponComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"

UCBossPattern_Slam::UCBossPattern_Slam()
{
	PatternId = FName("Slam");
}

void UCBossPattern_Slam::BeginDestroy()
{
	UE_LOG(LogTemp, Log, TEXT("[Slam] BeginDestroy called"));
	Cleanup();
	Super::BeginDestroy();
}

void UCBossPattern_Slam::ExecutePattern(int32 PhaseIndex, const FBossPatternDefinition& PatternData)
{
	Super::ExecutePattern(PhaseIndex, PatternData);

	UE_LOG(LogTemp, Warning, TEXT("[Slam] Executing slam attack - Phase %d"), PhaseIndex);
	UE_LOG(LogTemp, Log, TEXT("[Slam] ExecutionTime: %.2f, RecoveryTime: %.2f"), 
		PatternData.ExecutionTime, PatternData.RecoveryTime);

	if (!OwnerBoss.IsValid())  
	{
		UE_LOG(LogTemp, Error, TEXT("[Slam] Invalid OwnerBoss"));
		return;
	}

	CurrentPatternData = PatternData;

	float Duration = 0.0f;

	if (SlamMontage)
	{
		Duration = PlayMontage(SlamMontage);
		if (Duration <= 0.f) Duration = PatternData.ExecutionTime > 0.f ? PatternData.ExecutionTime : 1.5f;

		FTimerHandle TempTimer;
		GetWorld()->GetTimerManager().SetTimer(TempTimer, this, &UCBossPattern_Slam::PlayImpactEffectsAndDamage, Duration * 0.6f, false);
	}
	else
	{
		PlayImpactEffectsAndDamage();
		Duration = PatternData.ExecutionTime > 0.f ? PatternData.ExecutionTime : 1.0f;
	}

	float FinishTime = Duration + PatternData.RecoveryTime;
	GetWorld()->GetTimerManager().SetTimer(TH_Finish, this, &UCBossPattern_Slam::FinishSlam, FinishTime, false);
	
	UE_LOG(LogTemp, Log, TEXT("[Slam] Pattern will finish in %.2f seconds"), FinishTime);
}

void UCBossPattern_Slam::OnPatternEnd()
{
	Super::OnPatternEnd();

	UE_LOG(LogTemp, Log, TEXT("[Slam] Pattern ended"));
}

void UCBossPattern_Slam::Cleanup()
{
	Super::Cleanup();
	if(GetWorld()) GetWorld()->GetTimerManager().ClearTimer(TH_Finish);
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

	const FVector ImpactLocation = OwnerBoss->GetActorLocation();

	UGameplayStatics::ApplyRadialDamage(
		World,
		SlamDamage,
		ImpactLocation,
		DamageRadius,
		nullptr, 
		TArray<AActor*>(), 
		OwnerBoss.Get(),
		OwnerBoss->GetController(),
		true 
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