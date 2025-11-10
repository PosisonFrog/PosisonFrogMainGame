// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "00_Character/02_Component/01_EnemyComponent/CBossPatternBase.h"
#include "CBossPattern_Rush.generated.h"

/**
 * 돌진(Rush) 패턴
 * 플레이어 방향으로 일직선 경로를 설정하여 빠르게 돌진합니다.
 * P1: 기본 돌진
 * P2: 캐스팅 시간 단축 (Warn -0.10s, Rec -0.20s)
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
	virtual void BeginDestroy() override;  // 추가

	/** Tick에서 호출 - 돌진 이동 처리 */
	void TickRushMovement(float DeltaTime);

	/** 돌진 중인지 확인 */
	FORCEINLINE bool IsRushing() const { return bIsRushing; }

	/** 돌진 시작 델리게이트 핸들러 */
	UFUNCTION()
	void HandleRushMovementStart();

	/** 돌진 종료 델리게이트 핸들러 */
	UFUNCTION()
	void HandleRushMovementStop();

protected:

	/** 돌진 애니메이션 몽타주 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Rush")
	TObjectPtr<UAnimMontage> RushMontage;

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

	/** 돌진 속도 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Rush")
	float RushSpeed  = 1000.0f;

	/** 목표 도착 판정 반경 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Rush")
	float RushAcceptanceRadius = 150.0f;
	

	/** 현재 경고 시간 */
	float CurrentTelegraphDuration;

	/** 현재 회복 시간 */
	float CurrentRecoveryDuration;

private:
	/** 돌진 중 플래그 */
	bool bIsRushing  = false;

	/** 돌진 목표 위치 */
	FVector RushTargetLocation;

	/** 타이머 핸들 */
	FTimerHandle RushDelayTimer;
	FTimerHandle RushMoveTimer;

	/** 돌진 시작 */
	void StartRush();



	/** 돌진 시 데미지 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Rush")
	float RushDamage = 20.0f;

	/** 돌진 시 플레이어를 밀어내는 힘 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|Rush")
	float RushLaunchPower = 1500.0f;
	
	// ...
	/** 데미지를 입힌 플레이어 목록 (중복 데미지 방지) */
	UPROPERTY()
	TSet<AActor*> DamagedPlayers;
};