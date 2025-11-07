// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CHitStopComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class POSISONFROG_API UCHitStopComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCHitStopComponent();

	// 히트 스탑 시작, Duration : 지속 시간, TimeScale 시간 배율
	bool StartHitStop(AActor* TargetActor, float Duration = 0.1f, float TimeScale = 0.0f);

	// 특정 액터가 히트 스탑 중인지 확인하기 위한 함수
	bool IsActorInHitStop(AActor* TargetActor) const;
	
	// 여러 액터에 동시에 히트 스탑 적용
	void StartMultipleHitStop(const TArray<AActor*>& TargetActors, float Duration = 0.1f, float TimeScale = 0.0f);

	// 플레이어와 피격된 적 모두에게 히트 스탑 적용
	void StartPlayerAndEnemyHitStop(AActor* Player, AActor* HitEnemy, float Duration = 0.1f, float TimeScale = 0.0f);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// 히트 스탑 종료
	void EndHitStop();

	void EndHitStopForActor(AActor* TargetActor);

	FTimerHandle HitStopTimer;

	// 원래 Time Dilation 값들 저장을 위한 변수
	TMap<TWeakObjectPtr<AActor>, float> OriginalTimeDilations;

	// 현재 히트 스탑 중인 액터들
	TSet<TWeakObjectPtr<AActor>> ActorsInHitStop;

	// 액터별 타이머 핸들
	TMap<TWeakObjectPtr<AActor>, FTimerHandle> ActorTimerHandles;

	UPROPERTY(EditAnywhere, Category = "HitStop|Settings", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float DefaultDuration = 0.1f;

	UPROPERTY(EditAnywhere, Category = "HitStop|Settings", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float DefaultTimeScale = 0.0f;

	UPROPERTY(EditAnywhere, Category = "HitStop|Debug")
	bool bShowDebugMessages = true;
};
