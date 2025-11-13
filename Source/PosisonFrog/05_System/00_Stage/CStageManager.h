// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CEnemySpawnZone.h"
#include "CStageManager.generated.h"

class ACPlayerCharacter;
class ACEnemySpawnZone;
class ACStageBarrier;
class ACCheckPoint;
class ACEnemyCharacterBase;
class ACBossStageBarrier;
class ACEnemyDirector;

struct FStageSpawnRequest
{
	int32 StageID = INDEX_NONE;
	bool bIsPreload = false;

	FStageSpawnRequest() = default;
	FStageSpawnRequest(int32 InStageID, bool bInIsPreload) : StageID(InStageID), bIsPreload(bInIsPreload) {}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStageCleared, int32, StageID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCheckPointActivated, ACCheckPoint*, CheckPoint, ACPlayerCharacter*, Player);

UCLASS()
class POSISONFROG_API ACStageManager : public AActor
{
	GENERATED_BODY()

public:
	ACStageManager();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// ──────────── 스테이지 제어 ────────────
	void StartStageSpawn(int32 StageID);
	void CheckStageComplete(int32 StageID);
	void PrepareForRespawn(int32 TargetStageID);

	// ──────────── 보스 배리어 중앙 제어 ────────────
	void RegisterBossBarrier(ACBossStageBarrier* Barrier);
	void OnBossBattleStartRequested();
	void OnBossDefeated();
	
	// ──────────── 상태 조회 ────────────
	int32 GetRemainingEnemies(int32 StageID) const;
	int32 GetCurrentStage() const { return CurrentStage; }
	bool IsStageCleared(int32 StageID) const;
	
	// ──────────── 이벤트 ────────────
	UPROPERTY() FOnStageCleared OnStageCleared;
	UPROPERTY() FOnCheckPointActivated OnCheckPointActivated;
	
private:
	// ──────────── 초기화 ────────────
	void CollectSpawnZones();
	void CollectBarriers();
	void CollectCheckpoints();
	void CollectBossBarrier();

	// ──────────── 스폰 로직 ────────────
	// 분산 스폰 시작
	void StartDistributedSpawn(int32 StageID, bool bIsPreload = false);

	// 스폰 배치 처리
	void ProcessSpawnBatch();

	// 다음 스폰 요청 처리
	void ProcessNextSpawnRequest();
	
	// 선제적 로딩된 적들 활성화
	void ActivatePreloadedStage(int32 StageID);

	// ──────────── 스테이지 완료 ────────────
	void OnStageComplete(int32 StageID);
	void OpenStageBarrier(int32 StageID);
	void ActivateStageCheckPoint(int32 StageID);

	// 보스 배리어 제어
	void OpenBossBarrier(int32 StageID);

	// ──────────── 적 추적 ────────────
	// 적 사망 콜백
	UFUNCTION()
	void OnEnemyDied(AActor* DeadActor);

	// 선제적 로딩 트리거 체크
	void CheckPreloadTrigger();

	void ResetEnemy(ACEnemyCharacterBase* Enemy, int32 StageID);


public:
	void HandleEnemySpawnedFromDirector(ACEnemyCharacterBase* Enemy, int32 StageID, bool bWasExistingActor);
	
private:
	void RegisterDirector();
	bool RequestSpawnThroughDirector(const FSpawnTransformInfo& SpawnInfo, int32 StageID);
	bool RequestExistingActivationThroughDirector(ACEnemyCharacterBase* Enemy, int32 StageID);
	void ReleaseEnemyFromDirector(ACEnemyCharacterBase* Enemy);
	void CancelDirectorRequests(int32 StageID);
	
	// ──────────── 메모리 정리 ────────────
	bool IsSpawnInProgress() const;
	void QueueSpawnRequest(int32 StageID, bool bIsPreload);
	void ResetSpawnState();
	
public:
	void ClearAllEnemies();

private:
	// ──────────── 데이터 저장 ────────────
	TMap<int32, TArray<TObjectPtr<ACEnemySpawnZone>>> StageSpawnZones;
	TMap<int32, TArray<TObjectPtr<ACEnemyCharacterBase>>> StageEnemies;
	
	UPROPERTY(VisibleAnywhere, Category = "Stage|Info")
	TMap<int32, TObjectPtr<ACStageBarrier>> StageBarriers;
	
	UPROPERTY(VisibleAnywhere, Category = "Stage|Info")
	TMap<int32, TObjectPtr<ACCheckPoint>> StageCheckPoints;

	UPROPERTY(VisibleAnywhere, Category = "Stage|Info")
	TSet<int32> ClearedStages;

	UPROPERTY(VisibleAnywhere, Category = "Stage|Info")
	int32 CurrentStage = 1;

	UPROPERTY(VisibleAnywhere, Category = "Stage|Info")
	TObjectPtr<ACBossStageBarrier> BossBarrier;
	
	// ──────────── 분산 스폰 상태 ────────────
	int32 SpawningStage = -1;
	bool bIsPreloading = false;
	int32 SpawnProgress = 0;
	int32 SpawnPerFrame = 0;
	
	TArray<FSpawnTransformInfo> CurrentSpawnQueue;

	TMap<int32, TArray<TObjectPtr<ACEnemyCharacterBase>>> PreloadedEnemies;
	TSet<int32> PreloadedStages;

	TArray<FStageSpawnRequest> SpawnRequestQueue;
	
	// 분산 스폰에서 사용할 타이머
	FTimerHandle SpawnTimer;

public:
	// ──────────── 설정 ────────────
	// 선제적 로딩 트리거 (남은 적 수)
	UPROPERTY(EditAnywhere, Category = "Stage|Settings")
	int32 PreloadTriggerCount = 10;

	// 분산 스폰 분할 수
	UPROPERTY(EditAnywhere, Category = "Stage|Settings", meta = (ClapMin = "1"))
	int32 SpawnDivision = 5;

	// 게임 시작 시 자동 스폰할 스테이지
	UPROPERTY(EditAnywhere, Category = "Stage|Settings")
	int32 StartStage = 1;

	// 분산 스폰 간격
	float SpawnInterval = 0.016f;

	UPROPERTY(EditAnywhere, Category = "Stage|Debug")
	bool bEnableDebugLogs = false;

	bool bIsClearingEnemies = false;
	
	UPROPERTY()
	TObjectPtr<ACEnemyDirector> EnemyDirector;

private:
	// 핫픽스 변수
	UPROPERTY(EditAnywhere, Category = "Stage|Hotfix")
	float DeathHideDelay = 1.2f;

	TMap<int32, TArray<TWeakObjectPtr<ACEnemyCharacterBase>>> PendingBudgetActivations;
};
