// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "00_Character/02_Component/01_EnemyComponent/CBossPatternBase.h"
#include "03_Combat/Boss/BossPhaseDataAsset.h"
#include "CBossPattern_Barrage.generated.h"

class UParticleSystem;
class USoundBase;

/**
 * 폭격(Barrage) 패턴
 * 플레이어가 일정 거리 이상 떨어져 있을 때, 원거리 투사체 공격을 수행합니다.
 * P1: 기본 투사체 발사
 * P2: 투사체 증가 + 캐스팅 시간 단축 (Warn -0.10s, Rec -0.20s)
 */
UCLASS(EditInlineNew, DefaultToInstanced)
class POSISONFROG_API UCBossPattern_Barrage : public UCBossPatternBase
{
	GENERATED_BODY()

public:
	UCBossPattern_Barrage();

	virtual void ExecutePattern(int32 PhaseIndex, const FBossPatternDefinition& PatternData) override;
	virtual void OnPatternEnd() override;
	virtual void Cleanup() override;
	virtual void BeginDestroy() override;

protected:
	/** 폭격 애니메이션 몽타주 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Barrage")
	TObjectPtr<UAnimMontage> BarrageMontage;

	/** 투사체 클래스 (코코넛) */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Barrage")
	TSubclassOf<AActor> ProjectileClass;

	/** 경고 데칼 클래스 (붉은 원형) */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Barrage")
	TSubclassOf<AActor> WarningDecalClass;

	/** 발사 간격 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Barrage")
	float ShotInterval = 0.1f;

	/** 데칼 미리 표시 시간 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Barrage")
	float DecalPreviewTime = 1.0f;

	/** 코코넛 떨어지는 딜레이 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Barrage")
	float CoconutFallDelay  = 1.5f;

	/** 플레이어 근처 랜덤 범위 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Barrage")
	float RandomSpawnRadius = 1000.0f;

	/** 경고 데칼 반경 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Barrage")
	float WarningDecalRadius = 200.0f;
	
	/** 코코넛 떨어지는 속도 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Barrage")
	float FallSpeed = 20000.0f;

	/** 코코넛 떨어지기 시작하는 높이 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Barrage")
	float DropHeight = 2500.0f;

private:
	FBossPatternDefinition CurrentPatternData;
	
	int32 CurrentMaxShots = 15;
	int32 BarrageShotCount = 0;
	TArray<FVector> PrePlannedDropLocations;

	FTimerHandle BarrageLoopTimer;
	TArray<FTimerHandle> CoconutSpawnTimers;
	FTimerHandle TH_FinishDelay;
	
	void FinishBarrage();
	void StartBarrage();
	
	UFUNCTION()
	void FireBarrageShot();
	/** 모든 타이머 정리 */
	void ClearAllTimers();
};