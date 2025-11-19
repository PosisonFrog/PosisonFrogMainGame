// Fill out your copyright notice in the Description page of Project Settings.

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
 * 플레이어 근접 시, 지면을 강하게 내려쳐 범위 공격을 수행합니다.
 * P1: 기본 파쇄
 * P2: 캐스팅 시간 단축 (Warn -0.10s, Rec -0.20s)
 */
UCLASS(EditInlineNew, DefaultToInstanced)
class POSISONFROG_API UCBossPattern_Slam : public UCBossPatternBase
{
	GENERATED_BODY()

public:
	UCBossPattern_Slam();

	virtual void ExecutePattern(int32 PhaseIndex, const FBossPatternDefinition& PatternData) override;
	virtual void OnPatternEnd() override;
	virtual void Cleanup() override;
	virtual void BeginDestroy() override;

private:
	void FinishSlam();
	void PlayImpactEffectsAndDamage();

protected:
	/** 공격 애니메이션 몽타주 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Slam")
	TObjectPtr<UAnimMontage> SlamMontage;

	/** 바닥 충격 이펙트 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Slam|Effects")
	TObjectPtr<UParticleSystem> GroundImpactEffect;

	/** 바닥 충격 사운드 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Slam|Effects")
	TObjectPtr<USoundBase> GroundImpactSound;

	/** 카메라 흔들림 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Slam|Effects")
	TSubclassOf<UCameraShakeBase> GroundImpactShake;

	/** 내려찍기 데미지 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Slam")
	float SlamDamage = 30.0f;

	/** 데미지 반경 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Slam")
	float DamageRadius = 500.0f;

	/** 넉백 강도 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Slam")
	float SlamLaunchPower = 1000.0f;

private:
	FBossPatternDefinition CurrentPatternData;
	FTimerHandle TH_Finish;


};