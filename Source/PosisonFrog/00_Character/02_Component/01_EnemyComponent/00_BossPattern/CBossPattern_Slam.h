#pragma once

#include "CoreMinimal.h"
#include "00_Character/02_Component/01_EnemyComponent/CBossPatternBase.h"
#include "03_Combat/Boss/BossPhaseDataAsset.h"
#include "Camera/CameraShakeBase.h"
#include "CBossPattern_Slam.generated.h"

class UParticleSystem;
class USoundBase;

/**
 * 파쇄(Slam) 패턴
 */
UCLASS(ClassGroup=(Boss), meta=(BlueprintSpawnableComponent))
class POSISONFROG_API UCBossPattern_Slam : public UCBossPatternBase
{
	GENERATED_BODY()

public:
	UCBossPattern_Slam();

	virtual bool ExecutePattern(int32 PhaseIndex, const FBossPatternDefinition& PatternData) override;
	virtual void OnPatternEnd() override;
	virtual void Cleanup() override;

private:
	void FinishSlam();
	void PlayImpactEffectsAndDamage();
	void ClearTimers();

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Slam")
	TObjectPtr<UAnimMontage> SlamMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Slam|Effects")
	TObjectPtr<UParticleSystem> GroundImpactEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Slam|Effects")
	TObjectPtr<USoundBase> GroundImpactSound;

	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Slam|Effects")
	TSubclassOf<UCameraShakeBase> GroundImpactShake;

	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Slam")
	float SlamDamage = 30.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Slam")
	float DamageRadius = 600.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Slam")
	float SlamLaunchPower = 1000.0f;

private:
	FBossPatternDefinition CurrentPatternData;
	FTimerHandle TH_Finish;
	FTimerHandle TH_ImpactEffect;

	FVector LockedImpactLocation;
	bool bSavedOrientRotation = false;
	bool bSavedUseControllerDesiredRotation = false;
};