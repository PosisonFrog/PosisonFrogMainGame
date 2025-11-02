// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CBossPatternManager.generated.h"

class ACEnemyBossCharacter;
class UCEnemyBossPhaseComponent;
class UCEnemyWeaponComponent;
class AAIController;
struct FBossPhaseDefinition;
struct FBossPatternDefinition;

/**
 * 보스 패턴 실행을 전담하는 매니저 컴포넌트
 * BossPhaseComponent의 델리게이트를 바인딩하여 패턴별 로직을 C++에서 처리
 */
UCLASS(ClassGroup=(Boss), meta=(BlueprintSpawnableComponent))
class POSISONFROG_API UCBossPatternManager : public UActorComponent
{
	GENERATED_BODY()


public:

	UFUNCTION(BlueprintCallable, Category="AI|Chase")
	FORCEINLINE bool GetIsRushing() const { return bIsRushing; }
	
	UCBossPatternManager();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Pattern|Rush")
	void HandleRushMovementStart();

	UFUNCTION(BlueprintCallable, Category = "Pattern|Rush")
	void HandleRushMovementStop();

	/** AnimNotify에서 호출 - 현재 패턴 종료 알림 */
	UFUNCTION(BlueprintCallable, Category = "Pattern")
	void NotifyCurrentPatternEnd(bool bSuccess = true);
	
protected:
	/**============ 델리게이트 ============**/
	
	void BindToBossPhaseComponent();
	void UnbindFromBossPhaseComponent();

	/**============  이벤트 핸들  ============**/
	
	UFUNCTION()
	void HandlePhaseChanged(int32 PhaseIndex, const FBossPhaseDefinition& PhaseData);

	UFUNCTION()
	void HandlePatternStarted(int32 PhaseIndex, FName PatternId, const FBossPatternDefinition& PatternData, float RemainingPower);

	UFUNCTION()
	void HandlePatternFinished(int32 PhaseIndex, FName PatternId, const FBossPatternDefinition& PatternData, float RemainingPower);

	UFUNCTION()
	void HandleShoutStarted(int32 PhaseIndex, FName ShoutId, float Duration);


	/**============ 패턴 실행 함수들 ============**/
	
	// 기본 공격
	void ExecuteBasicAttack();

	/** 돌진 공격 */
	void ExecuteRushPattern();


	/** 내려찍기 */
	void ExecuteSlamPattern();

	/** 원거리 난사 */
	void ExecuteBarragePattern();
	void FireBarrageShot();
	void StopBarrage();
	


	/**============ 페이즈별 처리 ============**/
	
	/** 페이즈 전환 연출 */
	void PlayPhaseTransition(int32 PhaseIndex);

	/** 페이즈별 스탯 조정 */
	void UpdatePhaseStats(int32 PhaseIndex);


	
	/**============ 유틸리티 ============**/

	/** 몽타주 재생 및 완료 대기 */
	void PlayMontageAndNotify(UAnimMontage* Montage, bool bAutoNotifyFinish = true);

	/** 몽타주 완료 콜백 */
	UFUNCTION()
	void OnMontageCompleted(UAnimMontage* Montage, bool bInterrupted);

	/** 플레이어 타겟 가져오기 */
	AActor* GetPlayerTarget() const;

	/** AI 컨트롤러 가져오기 */
	AAIController* GetBossAI() const;
	


	
	/**============ 오너 & 컴포넌트 ============**/
	UPROPERTY()
	TObjectPtr<ACEnemyBossCharacter> OwnerBoss;

	UPROPERTY()
	TObjectPtr<UCEnemyBossPhaseComponent> PhaseComponent;

	UPROPERTY()
	TObjectPtr<UCEnemyWeaponComponent> WeaponComponent;


	
	/**============ 세팅 ============**/

	/** 패턴 ID와 공격 인덱스 매핑 */
	UPROPERTY(EditDefaultsOnly, Category="Pattern|Mapping")
	TMap<FName, int32> PatternAttackIndexMap;

	/** 기본 공격 인덱스 */
	UPROPERTY(EditDefaultsOnly, Category="Pattern|Mapping")
	int32 DefaultAttackIndex = 0;

	// Rush 설정
	UPROPERTY(EditDefaultsOnly, Category="Pattern|Rush")
	float RushTelegraphDuration = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category="Pattern|Rush")
	float RushSpeed = 1000.f;

	UPROPERTY(EditDefaultsOnly, Category="Pattern|Rush")
	float RushAcceptanceRadius = 150.f;


	
	// Barrage 설정
	UPROPERTY(EditDefaultsOnly, Category="Pattern|Barrage")
	TSubclassOf<AActor> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, Category="Pattern|Barrage")
	float BarrageShotInterval = 0.3f;

	UPROPERTY(EditDefaultsOnly, Category="Pattern|Barrage")
	int32 MaxBarrageShots = 10;

	UPROPERTY(EditDefaultsOnly, Category="Pattern|Barrage")
	float BarrageTotalDuration = 3.0f;
	

	// Phase 설정
	UPROPERTY(EditDefaultsOnly, Category="Phase")
	TArray<float> PhaseWalkSpeeds = {400.f, 500.f, 600.f};

	UPROPERTY(EditDefaultsOnly, Category="Phase")
	float PhaseTransitionInvulnerabilityDuration = 2.0f;

	

	/**============ 이펙트 사운드 ============**/

	UPROPERTY(EditDefaultsOnly, Category="Effects")
	TObjectPtr<UParticleSystem> PhaseChangeEffect;

	UPROPERTY(EditDefaultsOnly, Category="Effects")
	TObjectPtr<USoundBase> PhaseChangeSound;

	UPROPERTY(EditDefaultsOnly, Category="Effects")
	TObjectPtr<UParticleSystem> GroundImpactEffect;

	UPROPERTY(EditDefaultsOnly, Category="Effects")
	TSubclassOf<UCameraShakeBase> GroundImpactShake;

	

	FName CurrentPatternId;
	FVector RushTargetLocation;
	int32 BarrageShotCount;
	bool bIsPatternActive;
	bool bIsRushing;

	// 타이머 핸들
	FTimerHandle RushDelayTimer;
	FTimerHandle RushMoveTimer;
	FTimerHandle BarrageLoopTimer;
	FTimerHandle BarrageStopTimer;
	FTimerHandle GroundCheckTimer;
	FTimerHandle PhaseTransitionTimer;

	// 몽타주 종료 델리게이트
	FOnMontageEnded MontageEndDelegate;
	bool bShouldNotifyOnMontageEnd;
};