#pragma once

#include "CoreMinimal.h"
#include "00_Character/02_Component/01_EnemyComponent/CBossPatternBase.h"
#include "03_Combat/Boss/BossPhaseDataAsset.h"
#include "CBossPattern_Barrage.generated.h"

class UParticleSystem;
class USoundBase;

/**
 * 폭격(Barrage) 패턴
 */
UCLASS(ClassGroup=(Boss), meta=(BlueprintSpawnableComponent))
class POSISONFROG_API UCBossPattern_Barrage : public UCBossPatternBase
{
	GENERATED_BODY()

public:
	UCBossPattern_Barrage();

	virtual bool ExecutePattern(int32 PhaseIndex, const FBossPatternDefinition& PatternData) override;
	virtual void OnPatternEnd() override;
	virtual void Cleanup() override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Barrage")
	TObjectPtr<UAnimMontage> BarrageMontage;

	//데이터 에셋에서 관리되는 값
	UPROPERTY(VisibleAnywhere, Category = "Pattern|Barrage")
	TSubclassOf<AActor> ProjectileClass;

	//데이터 에셋에서 관리되는 값
	UPROPERTY(VisibleAnywhere, Category = "Pattern|Barrage")
	float FallSpeed = 0.0f;

	//데이터 에셋에서 관리되는 값
	UPROPERTY(VisibleAnywhere, Category = "Pattern|Barrage")
	float DropHeight = 0.0f;

	//데이터 에셋에서 관리되는 값
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Barrage")
	TSubclassOf<AActor> WarningDecalClass;

	//데이터 에셋에서 관리되는 값
	UPROPERTY(VisibleAnywhere, Category = "Pattern|Barrage")
	float RandomSpawnRadius = 0.0f;

	//데이터 에셋에서 관리되는 값
	UPROPERTY(VisibleAnywhere, Category = "Pattern|Barrage")
	float ShotInterval = 0.0f;

	

	//---------데칼 세팅 변수--------
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Barrage")
	float DecalPreviewTime = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Barrage")
	float CoconutFallDelay  = 1.5f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Barrage")
	float WarningDecalRadius = 200.0f;

	
	//----------데미지 변수 -------------
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Barrage")
	float BarrageDamage = 30.0f;
	
	
private:
	FBossPatternDefinition CurrentPatternData;
	
	int32 CurrentMaxShots = 0;
	int32 BarrageShotCount = 0;
	TArray<FVector> PrePlannedDropLocations;

	FTimerHandle BarrageLoopTimer;
	TArray<FTimerHandle> CoconutSpawnTimers;
	FTimerHandle TH_FinishDelay;
	
	void FinishBarrage();
	void StartBarrage();
	
	UFUNCTION()
	void FireBarrageShot();
	
	void ClearAllTimers();
};