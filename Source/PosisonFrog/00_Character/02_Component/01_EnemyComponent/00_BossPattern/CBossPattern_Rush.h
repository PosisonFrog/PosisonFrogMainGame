// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "00_Character/02_Component/01_EnemyComponent/CBossPatternBase.h"
#include "CBossPattern_Rush.generated.h"

class UAnimMontage;

/**
 * Rush 상태
 */
UENUM(BlueprintType)
enum class ERushState : uint8
{
	Idle,
	Telegraph,   // 경고 단계 (예고 애니메이션)
	Rushing,     // 실제 돌진 중
	Recovery,    // 돌진 종료 후 회복
	Cooldown     // 쿨다운 (패턴 종료 대기)
};

/**
 * Rush 종료 사유
 */
UENUM(BlueprintType)
enum class ERushEndReason : uint8
{
	None,
	ReachedTarget,    // 목표 지점 도달
	HitPlayer,        // 플레이어 충돌
	MaxTime,          // 최대 시간 초과
	Aborted           // 강제 중단
};

// 델리게이트 선언
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRushStateChanged, ERushState, NewState, ERushState, PreviousState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRushFinished, ERushEndReason, Reason, AActor*, HitActor);

/**
 * 돌진(Rush) 패턴 - Tanker Charge 스타일로 재구성
 * 
 * 실행 흐름:
 * 1. Telegraph: 경고 애니메이션 재생 + 플레이어 방향 회전
 * 2. Rushing: 애님 노티파이로 실제 돌진 시작 (Anim_RushStart 호출)
 * 3. Recovery: 돌진 종료 후 회복 동작
 * 4. Cooldown: 패턴 종료 처리
 */
UCLASS(EditInlineNew, DefaultToInstanced)
class POSISONFROG_API UCBossPattern_Rush : public UCBossPatternBase
{
	GENERATED_BODY()

public:
	UCBossPattern_Rush();

	virtual void ExecutePattern(int32 PhaseIndex) override;
	virtual void OnPatternEnd() override;
	virtual void Cleanup() override;
	virtual void UpdatePhaseSettings(int32 PhaseIndex) override;
	virtual void BeginDestroy() override;

	/** Tick에서 호출 - 돌진 이동 처리 */
	void TickRushMovement(float DeltaTime);

	/** 돌진 중인지 확인 */
	UFUNCTION(BlueprintPure, Category = "PF|BossPattern|Rush")
	bool IsRushing() const { return State == ERushState::Rushing; }

	/** 현재 상태 확인 */
	UFUNCTION(BlueprintPure, Category = "PF|BossPattern|Rush")
	ERushState GetRushState() const { return State; }

	/** 애님 노티파이: Telegraph → Rushing 전환 */
	UFUNCTION(BlueprintCallable, Category = "PF|BossPattern|Rush|Anim")
	void Anim_RushStart();

	/** 애님 노티파이: Recovery 종료 (선택사항) */
	UFUNCTION(BlueprintCallable, Category = "PF|BossPattern|Rush|Anim")
	void Anim_RecoveryEnd();

	/** 
	 * 구버전 호환성 메서드들 (Deprecated)
	 * 새로운 구조에서는 Anim_RushStart()를 사용하세요
	 */
	UFUNCTION(BlueprintCallable, Category = "PF|BossPattern|Rush|Deprecated")
	void HandleRushMovementStart();

	UFUNCTION(BlueprintCallable, Category = "PF|BossPattern|Rush|Deprecated")
	void HandleRushMovementStop();

public:
	// 델리게이트
	UPROPERTY(BlueprintAssignable)
	FOnRushStateChanged OnRushStateChanged;
	
	UPROPERTY(BlueprintAssignable)
	FOnRushFinished OnRushFinished;

protected:
	// ─────────────────────────────────────────────────────────────
	// Animation
	// ─────────────────────────────────────────────────────────────
	
	/** Telegraph 애니메이션 몽타주 (경고 동작) */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Rush|Animation")
	TObjectPtr<UAnimMontage> TelegraphMontage;

	/** Rush 애니메이션 몽타주 (돌진 동작) */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Rush|Animation")
	TObjectPtr<UAnimMontage> RushMontage;

	/** Recovery 애니메이션 몽타주 (회복 동작) */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Rush|Animation")
	TObjectPtr<UAnimMontage> RecoveryMontage;

	/** 애님 노티파이 없이도 자동으로 돌진 시작할지 여부 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Rush|Animation")
	bool bAutoStartOnTelegraphEnd = true;

	// ─────────────────────────────────────────────────────────────
	// Phase Settings
	// ─────────────────────────────────────────────────────────────

	/** P1 경고 시간 (예고) */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Rush|Phase1")
	float Phase1_TelegraphDuration = 1.0f;

	/** P1 회복 시간 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Rush|Phase1")
	float Phase1_RecoveryDuration = 1.5f;

	/** P2 경고 시간 (P1 - 0.10s) */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Rush|Phase2")
	float Phase2_TelegraphDuration = 0.9f;

	/** P2 회복 시간 (P1 - 0.20s) */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Rush|Phase2")
	float Phase2_RecoveryDuration = 1.3f;

	// ─────────────────────────────────────────────────────────────
	// Rush Movement
	// ─────────────────────────────────────────────────────────────

	/** 돌진 속도 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Rush|Movement", meta = (ClampMin = "400"))
	float RushSpeed = 1000.0f;

	/** 회전 속도 (도/초) */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Rush|Movement", meta = (ClampMin = "90"))
	float TurnRateDegPerSec = 360.f;

	/** 목표 도착 판정 반경 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Rush|Movement", meta = (ClampMin = "50"))
	float RushAcceptanceRadius = 150.0f;

	/** 최대 돌진 시간 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Rush|Movement", meta = (ClampMin = "0.5", ClampMax = "5.0"))
	float MaxRushTime = 2.0f;

	// ─────────────────────────────────────────────────────────────
	// Damage & Collision
	// ─────────────────────────────────────────────────────────────

	/** 돌진 시 데미지 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Rush|Damage", meta = (ClampMin = "0"))
	float RushDamage = 20.0f;

	/** 돌진 시 플레이어를 밀어내는 힘 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Rush|Damage", meta = (ClampMin = "0"))
	float RushLaunchPower = 1500.0f;

	/** 돌진 시 플레이어를 띄우는 수직 힘 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Rush|Damage", meta = (ClampMin = "0"))
	float RushLaunchUp = 300.0f;

	/** 충돌 감지 반경 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Rush|Collision", meta = (ClampMin = "20"))
	float CollisionRadius = 100.0f;

	/** 충돌 감지 전방 거리 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Rush|Collision", meta = (ClampMin = "60"))
	float CollisionTraceAhead = 120.0f;

	// ─────────────────────────────────────────────────────────────
	// Debug
	// ─────────────────────────────────────────────────────────────

	/** 디버그 드로우 활성화 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Rush|Debug")
	bool bDrawDebug = false;

private:
	/** 현재 상태 */
	UPROPERTY(Transient)
	ERushState State = ERushState::Idle;

	/** 현재 경고/회복 시간 */
	float CurrentTelegraphDuration = 1.0f;
	float CurrentRecoveryDuration = 1.5f;

	/** 돌진 목표 위치 */
	FVector RushTargetLocation = FVector::ZeroVector;

	/** 돌진 방향 */
	FVector RushDirection = FVector::ForwardVector;

	/** 돌진 시작 시간 */
	float RushStartTime = 0.f;

	/** 데미지를 입힌 플레이어 목록 (중복 데미지 방지) */
	TSet<TWeakObjectPtr<AActor>> DamagedPlayers;

	/** Movement 설정 백업 (Recovery 시 복구용) */
	float SavedMaxWalkSpeed = 400.0f;
	float SavedMaxAcceleration = 2048.0f;
	float SavedBrakingDeceleration = 2048.0f;
	float SavedGroundFriction = 8.0f;
	bool bSavedOrientRotationToMovement = true;

	/** 타이머 핸들 */
	FTimerHandle TH_Telegraph;
	FTimerHandle TH_MaxRush;
	FTimerHandle TH_Recovery;

	// ─────────────────────────────────────────────────────────────
	// Internal State Transitions
	// ─────────────────────────────────────────────────────────────

	void EnterState(ERushState NewState);
	void ResetTransientData();
	void ClearTimers();

	void BeginTelegraphInternal();
	void BeginRushingInternal();
	void EndRushingInternal(ERushEndReason Reason, AActor* HitActor = nullptr);
	void BeginRecoveryInternal(ERushEndReason Reason, AActor* HitActor = nullptr);
	void HandlePatternComplete();

	// ─────────────────────────────────────────────────────────────
	// Movement & Collision
	// ─────────────────────────────────────────────────────────────

	void UpdateRushing(float DeltaSeconds);
	void PerformCollisionTrace();
	bool SweepAhead(FHitResult& OutHit, float Distance) const;
	void FaceTowards(const FVector& Direction, float DeltaSeconds);
	void HandleMaxRushTime();

	// ─────────────────────────────────────────────────────────────
	// Validation
	// ─────────────────────────────────────────────────────────────

	bool HasValidOwner() const;
	float DistanceToTarget2D() const;
};