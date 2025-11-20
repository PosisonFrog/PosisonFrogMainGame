#include "CBossPattern_Slam.h"
#include "00_Character/01_Enemy/CEnemyBossCharacter.h"
#include "00_Character/02_Component/01_EnemyComponent/CEnemyWeaponComponent.h"
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

	// ✅ 유효성 검증
	if (!OwnerBoss.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[Slam] ExecutePattern REJECTED - Invalid OwnerBoss"));
		return false;
	}

	// ✅ 쿨다운 체크
	if (IsOnCooldown())
	{
		UE_LOG(LogTemp, Error, TEXT("[Slam] ExecutePattern REJECTED - Pattern is on cooldown"));
		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Slam] Executing slam attack - Phase %d"), PhaseIndex);
	UE_LOG(LogTemp, Log, TEXT("[Slam] ExecutionTime: %.2f, RecoveryTime: %.2f"), 
		PatternData.ExecutionTime, PatternData.RecoveryTime);

	CurrentPatternData = PatternData;

	// ✅ 기존 타이머 정리
	ClearTimers();

	float Duration = 0.0f;

	if (SlamMontage)
	{
		Duration = PlayMontage(SlamMontage);
		if (Duration <= 0.f) 
		{
			Duration = PatternData.ExecutionTime > 0.f ? PatternData.ExecutionTime : 1.5f;
		}

		// 임팩트 타이밍 (몽타주 60% 지점)
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				TH_ImpactEffect, 
				this, 
				&UCBossPattern_Slam::PlayImpactEffectsAndDamage, 
				Duration * 0.6f, 
				false
			);
		}
	}
	else
	{
		PlayImpactEffectsAndDamage();
		Duration = PatternData.ExecutionTime > 0.f ? PatternData.ExecutionTime : 1.0f;
	}

	// ✅ DataAsset의 RecoveryTime 사용
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
	
	return true;  // ✅ 성공 반환
}

void UCBossPattern_Slam::OnPatternEnd()
{
	Super::OnPatternEnd();

	// ✅ 타이머 정리
	ClearTimers();

	UE_LOG(LogTemp, Log, TEXT("[Slam] Pattern ended"));
}

void UCBossPattern_Slam::Cleanup()
{
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

	const FVector ImpactLocation = OwnerBoss->GetActorLocation();

	// 데미지 적용
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

	// 이펙트
	if (GroundImpactEffect)
	{
		UGameplayStatics::SpawnEmitterAtLocation(
			World,
			GroundImpactEffect,
			ImpactLocation,
			FRotator::ZeroRotator
		);
	}

	// 사운드
	if (GroundImpactSound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			World,
			GroundImpactSound,
			ImpactLocation
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
}