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


protected:

	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float DeltaTime) override;

private:
	/** 타겟 플레이어 */
	UPROPERTY()
	AActor* TargetPlayer;

	/** 업데이트 간격 */
	float UpdateInterval = 0.5f;
	float TimeSinceLastUpdate = 0.f;
	
};
