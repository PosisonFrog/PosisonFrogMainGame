// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "BossAIController.generated.h"

/**
 * 
 */
UCLASS()
class POSISONFROG_API ABossAIController : public AAIController
{
	GENERATED_BODY()

public:
	ABossAIController();
	
	UFUNCTION(BlueprintCallable, Category="AI")
	void SetTargetPlayer(AActor* NewTarget);

	/** 기본 추적 활성화/비활성화 (패턴 실행 시 사용) */
	UFUNCTION(BlueprintCallable, Category="AI")
	void SetChaseEnabled(bool bEnabled);
	

protected:

	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float DeltaTime) override;

private:
	/** 타겟 플레이어 */
	UPROPERTY()
	AActor* TargetPlayer;

	/** 회전 속도 (degree per second) */
	UPROPERTY(EditAnywhere, Category="AI")
	float RotationSpeed = 360.f;

	/** 추적을 시작할 거리 */
	UPROPERTY(EditAnywhere, Category="AI|Chase")
	float ChaseDistance = 500.f;

	/** 추적을 멈출 거리 (공격 사거리) */
	UPROPERTY(EditAnywhere, Category="AI|Chase")
	float StopDistance = 100.f;

	/** MoveTo 업데이트 주기 (초) */
	UPROPERTY(EditAnywhere, Category="AI|Chase")
	float MoveUpdateInterval = 0.5f;

	float TimeSinceMoveUpdate = 0.f;
	bool bIsMovingToTarget = false;
	
	/** 기본 추적 활성화 여부 (패턴 실행 중에는 false) */
	bool bChaseEnabled = false;  // 전투 시작 전에는 비활성화
	
	
};