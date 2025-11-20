// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CHitStopSubsystem.generated.h"

// ───────── 히트스톱 타겟 정보 구조체 ─────────
USTRUCT(BlueprintType)
struct FHitStopParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitStop", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float Duration = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitStop", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TimeScale = 0.0f;

	FHitStopParams() : Duration(0.1f), TimeScale(0.0f) {}
	FHitStopParams(float InDuration, float InTimeScale) : Duration(InDuration), TimeScale(InTimeScale) {}
};

/**
 * 
 */
UCLASS()
class POSISONFROG_API UCHitStopSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UCHitStopSubsystem();

	// ───────── Subsystem 인터페이스 ─────────
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ───────── 기본 히트스톱 함수 ─────────
	// 단일 액터에 히트스톱 적용
	bool StartHitStop(AActor* TargetActor, float Duration = 0.1f, float TimeScale = 0.0f);

	// 특정 액터가 히트스톱 중인지 확인
	bool IsActorInHitStop(AActor* TargetActor) const;
	
	// 여러 액터에 동시에 히트스톱 적용
	void StartMultipleHitStop(const TArray<AActor*>& TargetActors, float Duration = 0.1f, float TimeScale = 0.0f);

	// ───────── 플레이어/적 분리 히트스톱 ─────────
	// 플레이어와 피격된 적에게 서로 다른 히트스톱 값 적용
	void StartPlayerAndEnemyHitStop(
		AActor* Player, 
		AActor* HitEnemy, 
		const FHitStopParams& PlayerParams,
		const FHitStopParams& EnemyParams);

	// 간편 버전 (float 직접 전달)
	void StartPlayerAndEnemyHitStop(
		AActor* Player, 
		AActor* HitEnemy,
		float PlayerDuration, 
		float PlayerTimeScale,
		float EnemyDuration, 
		float EnemyTimeScale);

	// ───────── 유틸리티 ─────────
	// 모든 히트스톱 즉시 종료
	void EndAllHitStops();

private:
	// 개별 액터의 히트스톱 종료
	void EndHitStopForActor(AActor* TargetActor);

	// 원래 Time Dilation 값들 저장
	TMap<TWeakObjectPtr<AActor>, float> OriginalTimeDilations;

	// 현재 히트스톱 중인 액터들
	TSet<TWeakObjectPtr<AActor>> ActorsInHitStop;

	// 액터별 타이머 핸들
	TMap<TWeakObjectPtr<AActor>, FTimerHandle> ActorTimerHandles;

	// ───────── 디버그 설정 ─────────
	UPROPERTY(EditAnywhere, Category = "HitStop|Debug")
	bool bShowDebugMessages = false;
};
