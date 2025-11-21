// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CEnemySpawnZone.h"
#include "CTutorialManager.h"
#include "CStageManager.generated.h"

class ACRiotRobotHordeTrigger;
class ACPlayerCharacter;
class ACEnemySpawnZone;
class ACStageBarrier;
class ACCheckPoint;
class ACEnemyCharacterBase;
class ACBossStageBarrier;
class UCTutorialManager;

struct FStageSpawnRequest
{
	int32 StageID = INDEX_NONE;
	bool bIsPreload = false;

	FStageSpawnRequest() = default;
	FStageSpawnRequest(int32 InStageID, bool bInIsPreload) : StageID(InStageID), bIsPreload(bInIsPreload) {}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStageCleared, int32, StageID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCheckPointActivated, ACCheckPoint*, CheckPoint, ACPlayerCharacter*, Player);


USTRUCT(BlueprintType)
struct FStageFlowNode
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StageFlow")
	int32 NodeId = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StageFlow")
	FName TriggerTag;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StageFlow")
	FName TutorialStepId;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StageFlow")
	int32 StageSectionId = 1;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StageFlow")
	TArray<int32> ActivateSpawnStages;

	//노드 진입시 여는 베리어
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StageFlow")
	TArray<int32> OpenBarriers;

	//노드 완료(클리어) 시 여는 배리어
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StageFlow")
	TArray<int32> OpenBarriersOnComplete;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StageFlow")
	TArray<int32> CloseBarriers;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StageFlow")
	int32 NextNodeId = INDEX_NONE;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StageFlow")
	bool bAutoStart = false;
};

USTRUCT(BlueprintType)
struct FTutorialSpawnRule
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	FName StepId;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	TSubclassOf<ACEnemyCharacterBase> EnemyClass;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial", meta = (ClampMin = "1"))
	int32 EnemyCount = 5;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	bool bForceUltReady = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	bool bFillFuryToMax = false;
};

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
	void StartStageSpawn(int32 StageID, bool bForceImmediate = false);
	void CheckStageComplete(int32 StageID);
	void PrepareForRespawn(int32 TargetStageID);
	void RegisterHordeEnemy(ACEnemyCharacterBase* Enemy, int32 StageID);

	// ──────────── 트리거 & 튜토리얼 ────────────
	void HandleTrigger(FName TriggerTag);
	
	UFUNCTION()
	void OnTutorialStepCompleted(FName StepId);

	// ──────────── 보스 배리어 중앙 제어 ────────────
	void RegisterBossBarrier(ACBossStageBarrier* Barrier);
	void OnBossBattleStartRequested();
	void OnBossDefeated();
	void ResetBossBattleForRespawn();
	
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
	void CollectHordeTriggers();  // ← HordeTrigger 기능
	
	// ──────────── 튜토리얼 플로우 ────────────
	void InitializeStageFlow();
	void EnterNode(int32 NodeId);
	void AdvanceToNode(int32 NodeId);
	int32 FindNodeIndexById(int32 NodeId) const;
	void ApplyTutorialSetup(const FStageFlowNode& Node);
	FTutorialSpawnRule GetSpawnRuleForStep(FName StepId) const;
	void SpawnTutorialEnemies(int32 StageID, TSubclassOf<ACEnemyCharacterBase> EnemyClass, int32 Count);
	void FillPlayerForRule(const FTutorialSpawnRule& Rule);

	// ──────────── 스폰 로직 ────────────
	void StartDistributedSpawn(int32 StageID, bool bIsPreload = false, bool bForceImmediate = false);
	void ProcessSpawnBatch();
	void ProcessNextSpawnRequest();
	void ActivatePreloadedStage(int32 StageID);

	// ──────────── 스테이지 완료 ────────────
	void OnStageComplete(int32 StageID);
	void OpenStageBarrier(int32 StageID);
	void ActivateStageCheckPoint(int32 StageID);
	void OpenBossBarrier(int32 StageID);

	// ──────────── 적 추적 ────────────
	UFUNCTION()
	void OnEnemyDied(AActor* DeadActor);
	void CheckPreloadTrigger();
	void ResetEnemy(ACEnemyCharacterBase* Enemy);
	
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
	TMap<int32, TArray<TObjectPtr<ACRiotRobotHordeTrigger>>> StageHordeTriggers;  // ← HordeTrigger 기능

	UPROPERTY(EditAnywhere, Category = "Stage|Flow")
	TArray<FStageFlowNode> StageFlowNodes;
	
	UPROPERTY(VisibleAnywhere, Category = "Stage|Info")
	TMap<int32, TObjectPtr<ACStageBarrier>> StageBarriers;
	
	UPROPERTY(VisibleAnywhere, Category = "Stage|Info")
	TMap<int32, TObjectPtr<ACCheckPoint>> StageCheckPoints;

	UPROPERTY(VisibleAnywhere, Category = "Stage|Info")
	TSet<int32> ClearedStages;

	UPROPERTY(VisibleAnywhere, Category = "Stage|Info")
	int32 CurrentStage = 1;

	UPROPERTY(VisibleAnywhere, Category = "Stage|Info")
	int32 CurrentNodeId = INDEX_NONE;
	
	UPROPERTY(VisibleAnywhere, Category = "Stage|Info")
	TObjectPtr<ACBossStageBarrier> BossBarrier;
	
	UPROPERTY()
	TObjectPtr<UCTutorialManager> TutorialManager;
	
	// ──────────── 분산 스폰 상태 ────────────
	int32 SpawningStage = -1;
	bool bIsPreloading = false;
	int32 SpawnProgress = 0;
	int32 SpawnPerFrame = 0;
	
	TArray<FSpawnTransformInfo> CurrentSpawnQueue;

	TMap<int32, TArray<TObjectPtr<ACEnemyCharacterBase>>> PreloadedEnemies;
	TSet<int32> PreloadedStages;

	TArray<FStageSpawnRequest> SpawnRequestQueue;
	
	FTimerHandle SpawnTimer;

public:
	// ──────────── 설정 ────────────

	UPROPERTY(EditAnywhere, Category = "Stage|Tutorial")
	TSubclassOf<UCTutorialPopupWidget> TutorialPopupWidgetClass;

	UPROPERTY(EditAnywhere, Category = "Stage|Tutorial")
	TArray<FTutorialStep> TutorialSteps;
	
	UPROPERTY(EditAnywhere, Category = "Stage|Settings")
	int32 PreloadTriggerCount = 10;
	
	UPROPERTY(EditAnywhere, Category = "Stage|Flow")
	int32 StartNode = 0;
	
	UPROPERTY(EditAnywhere, Category = "Stage|Flow")
	FName TutorialSequenceId = TEXT("Stage1");
	
	UPROPERTY(EditAnywhere, Category = "Stage|Tutorial")
	int32 TutorialEnemyCount = 5;
	
	UPROPERTY(EditAnywhere, Category = "Stage|Tutorial")
	TSubclassOf<ACEnemyCharacterBase> DefaultTutorialEnemyClass;
	
	UPROPERTY(EditAnywhere, Category = "Stage|Tutorial")
	TSubclassOf<ACEnemyCharacterBase> TankerEnemyClass;
	
	UPROPERTY(EditAnywhere, Category = "Stage|Tutorial")
	TArray<FTutorialSpawnRule> TutorialSpawnRules;

	UPROPERTY(EditAnywhere, Category = "Stage|Settings", meta = (ClampMin = "1"))
	int32 SpawnDivision = 5;

	UPROPERTY(EditAnywhere, Category = "Stage|Settings")
	int32 StartStage = 1;

	float SpawnInterval = 0.016f;

	UPROPERTY(EditAnywhere, Category = "Stage|Debug")
	bool bEnableDebugLogs = false;

private:
	UPROPERTY(EditAnywhere, Category = "Stage|Hotfix")
	float DeathHideDelay = 1.2f;

	bool bIsClearingEnemies = false;

	bool bIsCurrentTutorialStepFinished = false;
	
	
protected:
	//해당 스테이지 ID까지는 클리어해도 다음 스테이지가 자동으로 스폰되지 않습니다. (트리거 사용)
	UPROPERTY(EditAnywhere, Category = "Stage|Settings")
	int32 LastTutorialStageID = 105;

	void RespawnTutorialEnemies(int32 StageID, FName StepId);

	FTimerHandle RespawnTimerHandle;
};