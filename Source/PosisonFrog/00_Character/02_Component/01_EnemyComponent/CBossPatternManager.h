// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "03_Combat/Boss/BossPhaseDataAsset.h"
#include "CBossPatternManager.generated.h"

class ACEnemyBossCharacter;
class UCEnemyBossPhaseComponent;
class UCEnemyWeaponComponent;
class AAIController;
class UCBossPatternBase;

/**
 * 보스 패턴 실행을 전담하는 매니저 컴포넌트
 * BossPhaseComponent의 델리게이트를 바인딩하여 패턴별 로직을 각 패턴 클래스에 위임
 */
UCLASS(ClassGroup=(Boss), meta=(BlueprintSpawnableComponent))
class POSISONFROG_API UCBossPatternManager : public UActorComponent
{
	GENERATED_BODY()

public:
	UCBossPatternManager();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Pattern")
	void NotifyCurrentPatternEnd(bool bSuccess = true);

	UFUNCTION(BlueprintCallable, Category = "Pattern|Rush")
	void HandleRushMovementStart();

	/** AnimNotify에서 호출 - Rush 이동 종료 */
	UFUNCTION(BlueprintCallable, Category = "Pattern|Rush")
	void HandleRushMovementStop();
	
	UFUNCTION(BlueprintCallable, Category = "Pattern")
	void CleanupAllPatterns();

protected:
	/**============ 델리게이트 ============**/
	void BindToBossPhaseComponent();
	void UnbindFromBossPhaseComponent();

	/**============ 이벤트 핸들 ============**/
	UFUNCTION()
	void HandlePhaseChanged(int32 PhaseIndex, const FBossPhaseDefinition& PhaseData);

	UFUNCTION()
	void HandlePatternStarted(int32 PhaseIndex, FName PatternId, const FBossPatternDefinition& PatternData, float RemainingPower);

	UFUNCTION()
	void HandlePatternFinished(int32 PhaseIndex, FName PatternId, const FBossPatternDefinition& PatternData, float RemainingPower);

	UFUNCTION()
	void HandleShoutStarted(int32 PhaseIndex, FName ShoutId, float Duration);

	/**============ 패턴 관리 ============**/
	/** 패턴 ID로 패턴 객체 찾기 */
	UCBossPatternBase* FindPattern(FName PatternId) const;

	void InitializePatterns();

	/**============ 페이즈별 처리 ============**/
	/** 페이즈 전환 연출 */
	void PlayPhaseTransition(int32 PhaseIndex);

	/** 페이즈별 스탯 조정 */
	void UpdatePhaseStats(int32 PhaseIndex);

	AActor* GetPlayerTarget() const;
	AAIController* GetBossAI() const;

	/** 거리 기반 패턴 검증 */
	float GetDistanceToPlayer() const;
	bool ValidatePatternDistance(FName PatternId, float Distance) const;
	FName GetFallbackPattern(FName OriginalPattern, float Distance) const;

	/**============ 스폰 시스템 ============**/
	struct FBossSpawnedActorEntry
	{
		TWeakObjectPtr<AActor> Actor;
		bool bDestroyOnPatternEnd = false;
	};

	struct FBossSpawnedMinionEntry
	{
		TWeakObjectPtr<APawn> Pawn;
		bool bDestroyOnPatternEnd = false;
	};

	struct FBossUtilitySpawnRuntime
	{
		FBossPatternUtilitySpawnDefinition Definition;
		int32 SpawnedCount = 0;
		FTimerHandle TimerHandle;
	};

	struct FBossMinionSpawnRuntime
	{
		FBossPatternMinionSpawnDefinition Definition;
		FTimerHandle TimerHandle;
	};
	
	/** 패턴 스폰 처리 */
	void SpawnPatternActors(const FBossPatternDefinition& PatternData);
	void SpawnPatternWeapons(const FBossPatternDefinition& PatternData);
	void SpawnPatternUtilities(const FBossPatternDefinition& PatternData);
	void SpawnPatternMinions(const FBossPatternDefinition& PatternData);
	void CleanupPatternActors();
	void CleanupUtilitySpawnTimers();
	void CleanupMinionSpawnTimers();
	
	FTransform ResolveSpawnTransform(const FBossPatternSpawnTransform& SpawnTransform) const;
	
	void RegisterSpawnedActor(AActor* Actor, bool bDestroyOnPatternEnd, TArray<FBossSpawnedActorEntry>& Container);
	void RegisterSpawnedMinion(APawn* Pawn, bool bDestroyOnPatternEnd);
	void ApplyInitialVelocity(AActor* SpawnedActor, const FVector& InitialVelocity) const;
	void SpawnUtilityActorImmediate(const FBossPatternUtilitySpawnDefinition& Definition);
	void SpawnMinionBatch(const FBossPatternMinionSpawnDefinition& Definition);
	void HandleUtilitySpawnTimer(int32 RuntimeId);
	void HandleMinionSpawnTimer(int32 RuntimeId);
	void StartProjectileRain(const FBossPatternProjectileRainSettings& RainSettings);
	void HandleProjectileRainTick();
	void StopProjectileRain(bool bNotifyPatternEnd = false);
	void SpawnProjectileRainWave();

	/**============ 오너 & 컴포넌트 ============**/
	UPROPERTY()
	TObjectPtr<ACEnemyBossCharacter> OwnerBoss;

	UPROPERTY()
	TObjectPtr<UCEnemyBossPhaseComponent> PhaseComponent;

	UPROPERTY()
	TObjectPtr<UCEnemyWeaponComponent> WeaponComponent;

	/**============ 패턴 객체들 ============**/
	UPROPERTY(EditDefaultsOnly, Instanced, Category="Patterns")
	TMap<FName, TObjectPtr<UCBossPatternBase>> PatternMap;
	
	// 서브오브젝트로 생성하기 위한 포인터들
	UPROPERTY()
	TObjectPtr<UCBossPatternBase> BasicAttackPattern;
	UPROPERTY()
	TObjectPtr<UCBossPatternBase> BarragePattern;
	UPROPERTY()
	TObjectPtr<UCBossPatternBase> RushPattern;
	UPROPERTY()
	TObjectPtr<UCBossPatternBase> SlamPattern;


	/**============ 세팅 ============**/
	// Phase 설정
	UPROPERTY(EditDefaultsOnly, Category="Phase")
	TArray<float> PhaseWalkSpeeds = {400.f, 500.f, 600.f};

	UPROPERTY(EditDefaultsOnly, Category="Phase")
	float PhaseTransitionInvulnerabilityDuration = 2.0f;

	// 거리 기반 패턴 조건
	UPROPERTY(EditDefaultsOnly, Category="Pattern|Distance", meta=(ClampMin="100"))
	float CloseRangeThreshold = 500.0f;

	UPROPERTY(EditDefaultsOnly, Category="Pattern|Distance", meta=(ClampMin="500"))
	float MidRangeThreshold = 1500.0f;

	/**============ 이펙트 사운드 ============**/
	UPROPERTY(EditDefaultsOnly, Category="Effects")
	TObjectPtr<UParticleSystem> PhaseChangeEffect;

	UPROPERTY(EditDefaultsOnly, Category="Effects")
	TObjectPtr<USoundBase> PhaseChangeSound;

	/**============ 런타임 변수 ============**/
	FName CurrentPatternId;
	bool bIsPatternActive;

	// 타이머 핸들
	FTimerHandle PhaseTransitionTimer;
	FTimerHandle RushTimerHandle;

	// 스폰 시스템
	TArray<FBossSpawnedActorEntry> ActiveWeaponActors;
	TArray<FBossSpawnedActorEntry> ActiveUtilityActors;
	TArray<FBossSpawnedMinionEntry> ActiveMinions;

	TMap<int32, FBossUtilitySpawnRuntime> ActiveUtilitySpawnRuntimes;
	int32 UtilitySpawnRuntimeIdCounter = 0;

	TMap<int32, FBossMinionSpawnRuntime> ActiveMinionSpawnRuntimes;
	int32 MinionSpawnRuntimeIdCounter = 0;

	FBossPatternProjectileRainSettings ActiveProjectileRainSettings;
	bool bProjectileRainActive = false;
	int32 ProjectileRainWaveCounter = 0;
	FTimerHandle ProjectileRainTimerHandle;
};