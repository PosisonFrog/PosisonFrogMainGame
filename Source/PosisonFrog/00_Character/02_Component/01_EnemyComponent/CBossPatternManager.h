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

UENUM(BlueprintType)
enum class EBossManagerState : uint8
{
	Idle,       // 대기 중
	Executing,  // 패턴 실행 중
	Cooldown    // 패턴 종료 후 쿨다운 대기 중
};

UCLASS(ClassGroup=(Boss), meta=(BlueprintSpawnableComponent))
class POSISONFROG_API UCBossPatternManager : public UActorComponent
{
	GENERATED_BODY()

public:
	UCBossPatternManager();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** * ✅ [핵심 수정] 패턴이 종료되었음을 매니저에게 알림
	 * @param bApplyCooldown true면 패턴의 쿨다운 시간만큼 대기 후 상태 해제, false면 즉시 해제
	 */
	UFUNCTION(BlueprintCallable, Category = "Pattern")
	void NotifyCurrentPatternEnd(bool bApplyCooldown = true);

	/** Rush AnimNotify 대응용 (호환성 유지) */
	UFUNCTION(BlueprintCallable, Category = "Pattern|Rush")
	void HandleRushMovementStart();

	UFUNCTION(BlueprintCallable, Category = "Pattern|Rush")
	void HandleRushMovementStop();
	
	UFUNCTION(BlueprintCallable, Category = "Pattern")
	void CleanupAllPatterns();

protected:
	/**============ 델리게이트 바인딩 ============**/
	void BindToBossPhaseComponent();
	void UnbindFromBossPhaseComponent();

	/**============ 페이즈 컴포넌트 이벤트 핸들러 ============**/
	UFUNCTION()
	void HandlePhaseChanged(int32 PhaseIndex, const FBossPhaseDefinition& PhaseData);

	UFUNCTION()
	void HandlePatternStarted(int32 PhaseIndex, FName PatternId, const FBossPatternDefinition& PatternData, float RemainingPower);

	UFUNCTION()
	void HandlePatternFinished(int32 PhaseIndex, FName PatternId, const FBossPatternDefinition& PatternData, float RemainingPower);

	UFUNCTION()
	void HandleShoutStarted(int32 PhaseIndex, FName ShoutId, float Duration);

	/**============ 내부 로직 ============**/
	void InitializePatterns();
	UCBossPatternBase* FindPattern(FName PatternId) const;

	void OnCooldownFinished();
	void SelectNextPattern();

	/**============ 유틸리티 ============**/
	void PlayPhaseTransition(int32 PhaseIndex);
	void UpdatePhaseStats(int32 PhaseIndex);
	AActor* GetPlayerTarget() const;
	AAIController* GetBossAI() const;
	float GetDistanceToPlayer() const;
	bool ValidatePatternDistance(FName PatternId, float Distance) const;
	FName GetFallbackPattern(FName OriginalPattern, float Distance) const;

	/**============ 스폰 시스템 (구조체 정의) ============**/
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

private:
	/**============ 컴포넌트 참조 ============**/
	UPROPERTY()
	TObjectPtr<ACEnemyBossCharacter> OwnerBoss;

	UPROPERTY()
	TObjectPtr<UCEnemyBossPhaseComponent> PhaseComponent;

	UPROPERTY()
	TObjectPtr<UCEnemyWeaponComponent> WeaponComponent;

	/**============ 패턴 객체 관리 ============**/
	UPROPERTY(EditDefaultsOnly, Instanced, Category="Patterns")
	TMap<FName, TObjectPtr<UCBossPatternBase>> PatternMap;
	
	UPROPERTY()
	TObjectPtr<UCBossPatternBase> BasicAttackPattern;
	UPROPERTY()
	TObjectPtr<UCBossPatternBase> BarragePattern;
	UPROPERTY()
	TObjectPtr<UCBossPatternBase> RushPattern;
	UPROPERTY()
	TObjectPtr<UCBossPatternBase> SlamPattern;
	UPROPERTY()
	TObjectPtr<UCBossPatternBase> CurrentPattern;

	/**============ 설정값 ============**/
	UPROPERTY(EditDefaultsOnly, Category="Phase")
	TArray<float> PhaseWalkSpeeds = {400.f, 500.f, 600.f};

	UPROPERTY(EditDefaultsOnly, Category="Phase")
	float PhaseTransitionInvulnerabilityDuration = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category="Pattern|Distance", meta=(ClampMin="100"))
	float CloseRangeThreshold = 500.0f;

	UPROPERTY(EditDefaultsOnly, Category="Pattern|Distance", meta=(ClampMin="500"))
	float MidRangeThreshold = 2000.0f;

	UPROPERTY(EditDefaultsOnly, Category="Effects")
	TObjectPtr<UParticleSystem> PhaseChangeEffect;

	UPROPERTY(EditDefaultsOnly, Category="Effects")
	TObjectPtr<USoundBase> PhaseChangeSound;

	/**============ 런타임 상태 ============**/
	FName CurrentPatternId;
	bool bIsPatternActive; // PhaseComponent가 이 값을 보고 대기함
	
	EBossManagerState State = EBossManagerState::Idle;
	FTimerHandle CooldownTimerHandle;
	
	float MinGlobalCooldown = 0.1f;

	// 기타 타이머 및 상태
	FTimerHandle PhaseTransitionTimer;
	FTimerHandle RushTimerHandle;

	// 스폰 액터 관리 컨테이너
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