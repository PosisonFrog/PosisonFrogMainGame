#pragma once

#include "CoreMinimal.h"
#include "00_Character/02_Component/01_EnemyComponent/CBossPatternBase.h"
#include "03_Combat/Boss/BossPhaseDataAsset.h"
#include "CBossPattern_Rush.generated.h"

class UAnimMontage;

/**
 * Rush 상태
 */
UENUM(BlueprintType)
enum class ERushState : uint8
{
	Idle,
	Telegraph,
	Rushing,
	Recovery,
	Cooldown
};

/**
 * Rush 종료 사유
 */
UENUM(BlueprintType)
enum class ERushEndReason : uint8
{
	None,
	ReachedTarget,
	HitPlayer,
	MaxTime,
	Aborted
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRushStateChanged, ERushState, NewState, ERushState, PreviousState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRushFinished, ERushEndReason, Reason, AActor*, HitActor);

/**
 * 돌진(Rush) 패턴
 */
UCLASS(ClassGroup=(Boss), meta=(BlueprintSpawnableComponent))
class POSISONFROG_API UCBossPattern_Rush : public UCBossPatternBase
{
	GENERATED_BODY()

public:
	UCBossPattern_Rush();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	virtual bool ExecutePattern(int32 PhaseIndex, const FBossPatternDefinition& PatternData) override;
	virtual void OnPatternEnd() override;
	virtual void Cleanup() override;

	/** Tick에서 호출 - 돌진 이동 처리 */
	void TickRushMovement(float DeltaTime);

	/** 돌진 중인지 확인 */
	UFUNCTION(BlueprintPure, Category = "PF|BossPattern|Rush")
	bool IsRushing() const { return State == ERushState::Rushing; }

	/** 현재 상태 확인 */
	UFUNCTION(BlueprintPure, Category = "PF|BossPattern|Rush")
	ERushState GetRushState() const { return State; }

	/** 애님 노티파이: Telegraph -> Rushing 전환 */
	UFUNCTION(BlueprintCallable, Category = "PF|BossPattern|Rush|Anim")
	void Anim_RushStart();

	/** 애님 노티파이: Recovery 종료 */
	UFUNCTION(BlueprintCallable, Category = "PF|BossPattern|Rush|Anim")
	void Anim_RecoveryEnd();

	/** 구버전 호환성 메서드들 */
	UFUNCTION(BlueprintCallable, Category = "PF|BossPattern|Rush|Deprecated")
	void HandleRushMovementStart();

	UFUNCTION(BlueprintCallable, Category = "PF|BossPattern|Rush|Deprecated")
	void HandleRushMovementStop();

public:
	UPROPERTY(BlueprintAssignable)
	FOnRushStateChanged OnRushStateChanged;
	
	UPROPERTY(BlueprintAssignable)
	FOnRushFinished OnRushFinished;

protected:
	// Animation
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Rush|Animation")
	TObjectPtr<UAnimMontage> TelegraphMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Rush|Animation")
	TObjectPtr<UAnimMontage> RushMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Rush|Animation")
	TObjectPtr<UAnimMontage> RecoveryMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Rush|Animation")
	bool bAutoStartOnTelegraphEnd = true;

	// Rush Movement
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Rush|Movement", meta = (ClampMin = "400"))
	float RushSpeed = 1000.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Rush|Movement", meta = (ClampMin = "90"))
	float TurnRateDegPerSec = 360.f;

	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Rush|Movement", meta = (ClampMin = "50"))
	float RushAcceptanceRadius = 150.0f;

	// ❌ MaxRushTime 제거 - DataAsset의 ExecutionTime 사용
	// ❌ RushMissTimeout 제거 - 사용되지 않음

	// Damage & Collision
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Rush|Damage", meta = (ClampMin = "0"))
	float RushDamage = 20.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Rush|Damage", meta = (ClampMin = "0"))
	float RushLaunchPower = 1500.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Rush|Damage", meta = (ClampMin = "0"))
	float RushLaunchUp = 300.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Rush|Collision", meta = (ClampMin = "20"))
	float CollisionRadius = 150.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Rush|Collision", meta = (ClampMin = "60"))
	float CollisionTraceAhead = 250.0f;

	// Debug
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Rush|Debug")
	bool bDrawDebug = false;

private:
	UPROPERTY(Transient)
	ERushState State = ERushState::Idle;

	FBossPatternDefinition CurrentPatternData;

	FVector LockedRushDirection = FVector::ForwardVector;
	bool bDirectionLocked = false;
	float RushStartTime = 0.f;
	float RushElapsedTime = 0.f;
	
	TSet<TWeakObjectPtr<AActor>> DamagedPlayers;
	ERushEndReason LastEndReason = ERushEndReason::None;

	// Movement 설정 백업
	float SavedMaxWalkSpeed = 400.0f;
	float SavedMaxAcceleration = 2048.0f;
	float SavedBrakingDeceleration = 2048.0f;
	float SavedGroundFriction = 8.0f;
	bool bSavedOrientRotationToMovement = true;

	// 타이머 핸들
	FTimerHandle TH_Telegraph;
	FTimerHandle TH_MaxRush;
	FTimerHandle TH_Recovery;

	// Internal State Transitions
	void EnterState(ERushState NewState);
	void ResetTransientData();
	void ClearTimers();

	void BeginTelegraphInternal();
	void BeginRushingInternal();
	void EndRushingInternal(ERushEndReason Reason, AActor* HitActor = nullptr);
	void BeginRecoveryInternal(ERushEndReason Reason, AActor* HitActor = nullptr);
	void HandlePatternComplete();

	// Movement & Collision
	void UpdateRushing(float DeltaSeconds);
	void PerformCollisionTrace();
	bool SweepAhead(FHitResult& OutHit, float Distance) const;
	void CheckOverlappingActors();
	void HandleMaxRushTime();

	// Validation
	bool HasValidOwner() const;
};