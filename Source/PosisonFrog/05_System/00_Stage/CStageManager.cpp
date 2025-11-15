// Fill out your copyright notice in the Description page of Project Settings.


#include "CStageManager.h"

#include "AIController.h"
#include "BrainComponent.h"
#include "CBossStageBarrier.h"
#include "CCheckPoint.h"
#include "CEnemySpawnZone.h"
#include "CStageBarrier.h"
#include "00_Character/00_Player/CPlayerCharacter.h"
#include "00_Character/01_Enemy/CEnemyCharacterBase.h"
#include "00_Character/02_Component/CBaseHealthComponent.h"
#include "99_Util/CLog.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"

ACStageManager::ACStageManager()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ACStageManager::BeginPlay()
{
	Super::BeginPlay();

	CollectSpawnZones();
	CollectBarriers();
	CollectCheckpoints();
	CollectBossBarrier();

	if (StartStage > 0)
	{
		StartStageSpawn(StartStage);
	}
}

void ACStageManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearAllTimersForObject(this);

	SpawnRequestQueue.Empty();
	ClearAllEnemies();
	
	Super::EndPlay(EndPlayReason);
}

// ────────────────────────────────────────────────────────────────────────────
// 스테이지 컨트롤러
// ────────────────────────────────────────────────────────────────────────────
void ACStageManager::StartStageSpawn(int32 StageID)
{
	TArray<TObjectPtr<ACEnemySpawnZone>>* SpawnZones = StageSpawnZones.Find(StageID);
	if (!SpawnZones || SpawnZones->Num() == 0)
	{
		CLog::Log(FString::Printf(TEXT("[ACStageManager::StartStageSpawn] 스테이지에 스폰 포인트가 없음 (StageID : %d)"), StageID));
		return;
	}

	if (IsSpawnInProgress())
	{
		if (SpawningStage == StageID)
		{
			if (bIsPreloading)
			{
				QueueSpawnRequest(StageID, false);
			}
			else if (bEnableDebugLogs)
			{
				CLog::Log(FString::Printf(TEXT("[ACStageManager::StartStageSpawn] Stage %d already spawning."), StageID));
			}
			return;
		}

		QueueSpawnRequest(StageID, false);
		return;
	}
	
	if (PreloadedStages.Contains(StageID))
	{
		ActivatePreloadedStage(StageID);
		return;
	}

	StartDistributedSpawn(StageID,false);
}

void ACStageManager::CheckStageComplete(int32 StageID)
{
	int32 Remaining = GetRemainingEnemies(StageID);

	if (Remaining == 0)
	{
		OnStageComplete(StageID);
	}
}

void ACStageManager::PrepareForRespawn(int32 TargetStageID)
{
	ResetSpawnState();

	for (auto StageIterator = StageEnemies.CreateIterator(); StageIterator; ++StageIterator)
	{
		const int32 StageID = StageIterator.Key();
		if (StageID >= TargetStageID)
			continue;

		for (ACEnemyCharacterBase* Enemy : StageIterator.Value())
		{
			if (IsValid(Enemy))
				Enemy->Destroy();
		}

		StageIterator.RemoveCurrent();
	}

	for (auto PreloadIterator = PreloadedEnemies.CreateIterator(); PreloadIterator; ++PreloadIterator)
	{
		const int32 StageID = PreloadIterator.Key();
		if (StageID >= TargetStageID)
			continue;

		for (ACEnemyCharacterBase* Enemy : PreloadIterator.Value())
		{
			if (IsValid(Enemy))
				Enemy->Destroy();
		}

		PreloadIterator.RemoveCurrent();
	}

	for (auto FlagIterator = PreloadedStages.CreateIterator(); FlagIterator; ++FlagIterator)
	{
		if (*FlagIterator < TargetStageID)
			FlagIterator.RemoveCurrent();
	}

	bool bReactivated = false;

	if (TArray<TObjectPtr<ACEnemyCharacterBase>>* ActiveArray = StageEnemies.Find(TargetStageID))
	{
		if (ActiveArray->Num() > 0)
		{
			for (ACEnemyCharacterBase* Enemy : *ActiveArray)
			{
				if (IsValid(Enemy))
					ResetEnemy(Enemy);
			}
			bReactivated = true;
		}
	}

	if (!bReactivated && PreloadedEnemies.Contains(TargetStageID))
	{
		ActivatePreloadedStage(TargetStageID);
		bReactivated = true;
	}

	if (!bReactivated)
	{
		if (ClearedStages.Contains(TargetStageID))
		{
			if (bEnableDebugLogs)
			{
				CLog::Log(FString::Printf(TEXT("[ACStageManager::PrepareForRespawn] Stage %d already cleared. Skipping respawn."), TargetStageID));
			}
		}
		else
		{
			StartStageSpawn(TargetStageID);
		}
	}
	
	CurrentStage = TargetStageID;

	for (auto ClearIterator = ClearedStages.CreateIterator(); ClearIterator; ++ClearIterator)
	{
		if (*ClearIterator > TargetStageID)
			ClearIterator.RemoveCurrent();
	}
}


void ACStageManager::RegisterBossBarrier(ACBossStageBarrier* Barrier)
{
	if (!IsValid(Barrier))
	{
		CLog::Log(TEXT("[StageManager] RegisterBossBarrier: nullptr 전달됨"));
		return;
	}
	
	if (BossBarrier == Barrier)
	{
		CLog::Log(FString::Printf(TEXT("[StageManager] BossBarrier 이미 동일한 인스턴스로 등록됨: %s"), *Barrier->GetName()));
		return;
	}
	
	if (IsValid(BossBarrier))
	{
		CLog::Log(FString::Printf(TEXT("[StageManager] BossBarrier 이미 등록됨: %s"), *BossBarrier->GetName()));
		CLog::Log(FString::Printf(TEXT("[StageManager] 새로운 BossBarrier로 교체: %s"), *Barrier->GetName()));
	}

	BossBarrier = Barrier;
	CLog::Log(TEXT("================================================================="));
	CLog::Log(TEXT("[StageManager] BossBarrier 등록 완료"));
	CLog::Log(FString::Printf(TEXT("[StageManager] Barrier 이름: %s"), *BossBarrier->GetName()));
	CLog::Log(FString::Printf(TEXT("[StageManager] Trigger Stage ID: %d"), BossBarrier->TriggerStageID));
	CLog::Log(TEXT("================================================================="));
}

void ACStageManager::OnBossBattleStartRequested()
{
	CLog::Log(TEXT("================================================================="));
	CLog::Log(TEXT("[StageManager] 보스 전투 시작 요청 받음"));
 
	if (!IsValid(BossBarrier))
		CollectBossBarrier();
			
	if (!IsValid(BossBarrier))
	{
		CLog::Log(TEXT("[StageManager] BossBarrier가 등록되지 않음"));
		CLog::Log(TEXT("================================================================="));
		return;
	}
 
	CLog::Log(TEXT("[StageManager] BossBarrier 닫기 명령 전송"));
	BossBarrier->OnBossBattleStart();
	CLog::Log(TEXT("[StageManager] BossBarrier 닫기 완료"));
	CLog::Log(TEXT("================================================================="));
}

void ACStageManager::OnBossDefeated()
{
	CLog::Log(TEXT("================================================================="));
	CLog::Log(TEXT("[StageManager] 보스 사망 알림 받음"));
 
	if (!IsValid(BossBarrier))
		CollectBossBarrier();
			
	if (!IsValid(BossBarrier))
	{
		CLog::Log(TEXT("[StageManager] BossBarrier가 등록되지 않음"));
		CLog::Log(TEXT("================================================================="));
		return;
	}
 
	CLog::Log(TEXT("[StageManager] BossBarrier 열기 명령 전송"));
	BossBarrier->OnBossBattleEnd();
	CLog::Log(TEXT("[StageManager] BossBarrier 열기 완료 - 보스 구역 탈출 가능"));
	CLog::Log(TEXT("================================================================="));
}

void ACStageManager::ResetBossBattleForRespawn()
{
	if (!IsValid(BossBarrier))
		CollectBossBarrier();
	
	if (!IsValid(BossBarrier))
		return;

	if (BossBarrier->IsBossBattleActive() || !BossBarrier->IsOpen())
	{
		CLog::Log(TEXT("[StageManager] ResetBossBattleForRespawn - reopening boss barrier"));
		BossBarrier->OnBossBattleEnd();
	}
}

int32 ACStageManager::GetRemainingEnemies(int32 StageID) const
{
	if (const TArray<TObjectPtr<ACEnemyCharacterBase>>* Enemies = StageEnemies.Find(StageID))
	{
		int32 AliveCount = 0;
 
		for (const TObjectPtr<ACEnemyCharacterBase>& Enemy : *Enemies)
		{
			ACEnemyCharacterBase* EnemyPtr = Enemy.Get();
			
			if (!IsValid(EnemyPtr) || EnemyPtr->IsPendingKillPending())
				continue;
 
			const UCBaseHealthComponent* HealthComp = EnemyPtr->FindComponentByClass<UCBaseHealthComponent>();
			const bool bIsAlive = !HealthComp || !HealthComp->IsDead();
					
			if (bIsAlive)
				AliveCount++;
		}
 
		return AliveCount;
	}
	
	return 0;
}

bool ACStageManager::IsStageCleared(int32 StageID) const
{
	return ClearedStages.Contains(StageID);
}

void ACStageManager::CollectSpawnZones()
{
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACEnemySpawnZone::StaticClass(), FoundActors);

	for (AActor* Actor : FoundActors)
	{
		ACEnemySpawnZone* Zone = Cast<ACEnemySpawnZone>(Actor);
		if (!IsValid(Zone))
		{
			CLog::Log(FString::Printf(TEXT("[ACStageManager::CollectSpawnZones] Invalid Spawn zone : %s"), *Actor->GetName()));
			continue;
		}

		if (Zone->EnemyTypes.Num() == 0)
		{
			CLog::Log(FString::Printf(TEXT("[ACStageManager::CollectSpawnZones] WARNING: 스폰적에 적 타입이 없음 : %s"), *Actor->GetName()));
			continue;
		}

		StageSpawnZones.FindOrAdd(Zone->SectionID).Add(Zone);
	}
}

void ACStageManager::CollectBarriers()
{
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACStageBarrier::StaticClass(), FoundActors);

	for (AActor* Actor : FoundActors)
	{
		ACStageBarrier* Barrier = Cast<ACStageBarrier>(Actor);
		if (IsValid(Barrier))
			StageBarriers.Add(Barrier->SectionID, Barrier);
	}
}

void ACStageManager::CollectCheckpoints()
{
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACCheckPoint::StaticClass(), FoundActors);

	for (AActor* Actor : FoundActors)
	{
		ACCheckPoint* CheckPoint = Cast<ACCheckPoint>(Actor);
		if (IsValid(CheckPoint))
			StageCheckPoints.Add(CheckPoint->SectionID, CheckPoint);
	}
}

void ACStageManager::StartDistributedSpawn(int32 StageID, bool bIsPreload)
{
	if (IsSpawnInProgress())
	{
		if (SpawningStage == StageID && bIsPreloading == bIsPreload)
			return;

		QueueSpawnRequest(StageID, bIsPreload);
		return;
	}
	
	TArray<TObjectPtr<ACEnemySpawnZone>>* SpawnZones = StageSpawnZones.Find(StageID);
	if (!SpawnZones || SpawnZones->Num() == 0)
	{
		ProcessNextSpawnRequest();
		return;
	}

	TArray<FSpawnTransformInfo> SpawnInfos;
	int32 TotalCount = 0;

	for (ACEnemySpawnZone* Zone : *SpawnZones)
	{
		if (!IsValid(Zone))
			continue;

		TArray<FSpawnTransformInfo> ZoneSpawns = Zone->GenerateSpawnTransforms();
		TotalCount += ZoneSpawns.Num();

		SpawnInfos.Append(ZoneSpawns);

		CLog::Log(FString::Printf(TEXT("[ACStageManager::StartDistributedSpawn] 존 '%s' - %d 마리 생성"), *Zone->GetName(), ZoneSpawns.Num()));
	}

	if (SpawnInfos.Num() == 0)
	{
		CLog::Log(FString::Printf(TEXT("[ACStageManager::StartDistributedSpawn] 스테이지 %d에 유요한 스폰 정보가 없음"), StageID));
		ProcessNextSpawnRequest();
		return;
	}

	SpawnPerFrame = FMath::Max(1, TotalCount/SpawnDivision);
	SpawningStage = StageID;
	bIsPreloading = bIsPreload;
	SpawnProgress = 0;
	CurrentSpawnQueue = SpawnInfos;

	if (!bIsPreload)
		CurrentStage = StageID;

	GetWorldTimerManager().SetTimer(SpawnTimer, this, &ACStageManager::ProcessSpawnBatch, SpawnInterval, true);
}

void ACStageManager::ProcessSpawnBatch()
{
	if (SpawningStage == -1 || CurrentSpawnQueue.Num() == 0)
	{
		ResetSpawnState();
		ProcessNextSpawnRequest();
		return;
	}

	int32 TotalCount = CurrentSpawnQueue.Num();
	int32 BatchStart = SpawnProgress;
	int32 BatchEnd = FMath::Min(BatchStart + SpawnPerFrame, TotalCount);

	// 이번 배치에 스폰할 적들
	for (int32 i = BatchStart; i < BatchEnd; i++)
	{
		if (i >= CurrentSpawnQueue.Num())
			break;

		const FSpawnTransformInfo& SpawnInfo = CurrentSpawnQueue[i];
		// 타입 정보가 이미 포함되어 있음!
		if (!SpawnInfo.EnemyClass)
		{
			CLog::Log(TEXT("[ACStageManager::ProcessSpawnBatch] 경고: 스폰 큐에 null 클래스가 있음!"));
			continue;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		// SpawnInfo에서 Transform과 EnemyClass 모두 사용
		ACEnemyCharacterBase* Enemy = GetWorld()->SpawnActor<ACEnemyCharacterBase>(
			SpawnInfo.EnemyClass,      // 각 적마다 다른 타입
			SpawnInfo.Transform,
			SpawnParams);

		if (Enemy)
		{
			Enemy->SaveInitialTransform();
			
			if (bIsPreloading)
			{
				// 비활성화 (선제 로딩)
				Enemy->SetActorHiddenInGame(true);
				Enemy->SetActorEnableCollision(false);
				Enemy->SetActorTickEnabled(false);

				if (AAIController* AI = Cast<AAIController>(Enemy->GetController()))
				{
					AI->StopMovement();
					
					if (UBrainComponent* Brain = AI->GetBrainComponent())
					{
						Brain->PauseLogic(TEXT("Preload"));
					}
				}

				PreloadedEnemies.FindOrAdd(SpawningStage).Add(Enemy);
			}
			else
			{
				if (UCBaseHealthComponent* HealthComp = Enemy->FindComponentByClass<UCBaseHealthComponent>())
				{
					HealthComp->OnDeath.AddDynamic(this, &ACStageManager::OnEnemyDied);
				}
				StageEnemies.FindOrAdd(SpawningStage).Add(Enemy);
			}
		}
	}

	SpawnProgress = BatchEnd;

	// 스폰 완료 확인
	if (SpawnProgress >= TotalCount)
	{
		if (bIsPreloading)
		{
			PreloadedStages.Add(SpawningStage);
			CLog::Log(FString::Printf(TEXT("[ACStageManager::ProcessSpawnBatch] 스테이지 %d 선제 로딩 완료! (%d마리 대기 중)"),
				SpawningStage, PreloadedEnemies[SpawningStage].Num()));
		}
		else
		{
			CLog::Log(FString::Printf(TEXT("[ACStageManager::ProcessSpawnBatch] 스테이지 %d 스폰 완료! (%d마리 활성화)"),
				SpawningStage, StageEnemies[SpawningStage].Num()));
		}

		ResetSpawnState();
		ProcessNextSpawnRequest();
	}
}

void ACStageManager::ProcessNextSpawnRequest()
{
	if (IsSpawnInProgress())
		return;
	
	while (SpawnRequestQueue.Num() > 0)
	{
		FStageSpawnRequest Request = SpawnRequestQueue[0];
		SpawnRequestQueue.RemoveAt(0);
			
		if (!StageSpawnZones.Contains(Request.StageID))
			continue;
			
		if (Request.bIsPreload)
		{
			if (PreloadedStages.Contains(Request.StageID))
				continue;
					
			StartDistributedSpawn(Request.StageID, true);
		}
		else
		{
			StartStageSpawn(Request.StageID);
		}
			
		if (IsSpawnInProgress())
			break;
	}
}

void ACStageManager::ActivatePreloadedStage(int32 StageID)
{
	TArray<TObjectPtr<ACEnemyCharacterBase>>* PreloadArray = PreloadedEnemies.Find(StageID);
	if (!PreloadArray || PreloadArray->Num() == 0)
	{
		CLog::Log(TEXT("[ACStageManager::ActivatePreloadedStage] 선제 로딩된 적이 없음"));
		return;
	}

	TArray<TObjectPtr<ACEnemyCharacterBase>>& EnemyArray = StageEnemies.FindOrAdd(StageID);
	EnemyArray.Empty();
	EnemyArray.Reserve(PreloadArray->Num());

	int32 ActivatedCount = 0;

	for (ACEnemyCharacterBase* Enemy : *PreloadArray)
	{
		if (!IsValid(Enemy))
			continue;

		FVector CurrentLocation = Enemy->GetActorLocation();
		if (CurrentLocation.Z < 0.0f)
		{
			UE_LOG(LogTemp, Error, TEXT("[%s] Z가 너무 낮음! %.1f, 보정중"), 
				*Enemy->GetName(), CurrentLocation.Z);
        
			CurrentLocation.Z = 100.0f;
			Enemy->SetActorLocation(CurrentLocation);
			Enemy->SaveInitialTransform();  // 다시 저장
		}
		
		// 활성화
		Enemy->SetActorHiddenInGame(false);
		Enemy->SetActorEnableCollision(true);
		Enemy->SetActorTickEnabled(true);

		if (AAIController* AI = Cast<AAIController>(Enemy->GetController()))
		{
			if (UPathFollowingComponent* PathComp = AI->FindComponentByClass<UPathFollowingComponent>())
			{
				PathComp->AbortMove(*AI, FPathFollowingResultFlags::OwnerFinished);
				PathComp->OnRequestFinished.Clear();
			}

			if (UBrainComponent* Brain = AI->GetBrainComponent())
			{
				Brain->StopLogic(TEXT("Activate"));
				Brain->RestartLogic();
			}

			AI->UnPossess();
			AI->Possess(Enemy);
		}
		
		else
		{
			if (UWorld* World = Enemy->GetWorld())
				Enemy->SpawnDefaultController();
		}
		
		if (UCBaseHealthComponent* HealthComp = Enemy->FindComponentByClass<UCBaseHealthComponent>())
		{
			HealthComp->OnDeath.AddDynamic(this, &ACStageManager::OnEnemyDied);
		}
		if (UWorld* World = Enemy->GetWorld())
		{
			TWeakObjectPtr<ACEnemyCharacterBase> WeakEnemy = Enemy;
			World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakEnemy]()
			{
					if (WeakEnemy.IsValid())
					{
							WeakEnemy->OnRespawned();
					}
			}));
		}
		else
		{
			Enemy->OnRespawned();
		}
		EnemyArray.Add(Enemy);
		ActivatedCount++;
	}
	PreloadedEnemies.Remove(StageID);
	PreloadedStages.Remove(StageID);
	CurrentStage = StageID;
}

void ACStageManager::OnStageComplete(int32 StageID)
{
	CLog::Log(TEXT("================================================================="));
	CLog::Log(TEXT("[StageManager] DEBUG - OnStageComplete 호출됨"));
	CLog::Log(FString::Printf(TEXT("[StageManager] StageID: %d"), StageID));
	CLog::Log(FString::Printf(TEXT("[StageManager] ClearedStages에 이미 포함? %s"), 
		ClearedStages.Contains(StageID) ? TEXT("Yes - 무시됨") : TEXT("No - 진행")));
	
	// 중요: 중복 호출 방지
	if (ClearedStages.Contains(StageID))
	{
		CLog::Log(TEXT("[StageManager] 이미 클리어된 스테이지 - 중복 호출 감지"));
		CLog::Log(TEXT("================================================================="));
		return;
	}

	CLog::Log(TEXT("[StageManager] 1단계: ClearedStages에 추가 중"));
	ClearedStages.Add(StageID);
	CLog::Log(TEXT("[StageManager] ClearedStages에 추가 완료"));

	CLog::Log(TEXT("[StageManager] 2단계: ActivateStageCheckPoint 호출 중"));
	ActivateStageCheckPoint(StageID);
	
	CLog::Log(TEXT("[StageManager] 3단계: OpenStageBarrier 호출 중 (일반 배리어)"));
	OpenStageBarrier(StageID);
	
	// 핵심 변경: 보스 배리어 체크 추가
	CLog::Log(TEXT("[StageManager] 4단계: OpenBossBarrier 호출 중 (보스 배리어)"));
	OpenBossBarrier(StageID);
	
	CLog::Log(TEXT("[StageManager] 5단계: OnStageCleared 델리게이트 브로드캐스트 중"));
	CLog::Log(FString::Printf(TEXT("[StageManager] OnStageCleared 구독자 있음? %s"), OnStageCleared.IsBound() ? TEXT("Yes") : TEXT("No")));
	OnStageCleared.Broadcast(StageID);
	CLog::Log(TEXT("[StageManager] OnStageCleared 브로드캐스트 완료"));

	// 다음 스테이지 처리
	int32 NextStage = StageID + 1;
	if (PreloadedStages.Contains(NextStage))
	{
		CLog::Log(FString::Printf(TEXT("[StageManager] 다음 스테이지 (%d) 이미 로딩되어 있음 - 활성화 중"), NextStage));
		ActivatePreloadedStage(NextStage);
	}
	else if (StageSpawnZones.Contains(NextStage))
	{
		CLog::Log(FString::Printf(TEXT("[StageManager] 다음 스테이지 (%d) 스폰 시작"), NextStage));
		StartStageSpawn(NextStage);
	}
	else
	{
		CLog::Log(FString::Printf(TEXT("[StageManager] 다음 스테이지 (%d) 없음 - 마지막 스테이지 또는 보스 스테이지"), NextStage));
	}
	
	CLog::Log(TEXT("================================================================="));
}

void ACStageManager::OpenStageBarrier(int32 StageID)
{
	if (auto Barrier = StageBarriers.Find(StageID))
	{
		if (IsValid(*Barrier))
		{
			(*Barrier)->OpenBarrier();
			CLog::Log(FString::Printf(TEXT("[ACStageManager::OpenStageBarrier] 스테이지 %d 장벽 개방"), StageID));
		}
	}
	else
	{
		CLog::Log(FString::Printf(TEXT("[ACStageManager::OpenStageBarrier] 스테이지 %d 장벽 없음"), StageID));
	}
}

void ACStageManager::ActivateStageCheckPoint(int32 StageID)
{
	if (auto CheckPoint = StageCheckPoints.Find(StageID))
	{
		if (IsValid(*CheckPoint))
		{
			(*CheckPoint)->ActivateCheckPoint();
			CLog::Log(FString::Printf(TEXT("[ACStageManager::ActivateStageCheckPoint] 스테이지 %d 체크포인트 활성화"), StageID));

			// GameMode에 알림 (필요시 구현)
			if (ACPlayerCharacter* Player = Cast<ACPlayerCharacter>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0)))
			{
				OnCheckPointActivated.Broadcast(*CheckPoint, Player);
				CLog::Log(TEXT("[ACStageManager::ActivateStageCheckPoint] 체크포인트 활성화 이벤트 발행"));
			}
		}
	}
	else
	{
		CLog::Log(FString::Printf(TEXT("[ACStageManager::ActivateStageCheckPoint] 스테이지 %d 체크포인트 없음"), StageID));
	}
}

void ACStageManager::OpenBossBarrier(int32 StageID)
{
	CLog::Log(TEXT("================================================================="));
	CLog::Log(TEXT("[StageManager] DEBUG - OpenBossBarrier 호출됨"));
	CLog::Log(FString::Printf(TEXT("[StageManager] StageID: %d"), StageID));

	if (!IsValid(BossBarrier))
		CollectBossBarrier();

	if (!IsValid(BossBarrier))
	{
		CLog::Log(TEXT("[StageManager] BossBarrier가 등록되지 않음"));
		CLog::Log(TEXT("[StageManager] 보스 스테이지가 없는 레벨이거나"));
		CLog::Log(TEXT("[StageManager] BossBarrier가 RegisterBossBarrier()를 호출하지 않음"));
		CLog::Log(TEXT("================================================================="));
		return;
	}
 
	CLog::Log(FString::Printf(TEXT("[StageManager] BossBarrier TriggerStageID: %d"), BossBarrier->TriggerStageID));
	CLog::Log(FString::Printf(TEXT("[StageManager] 일치 여부: %s"), 
		StageID == BossBarrier->TriggerStageID ? TEXT("일치") : TEXT("불일치")));
 
	if (StageID == BossBarrier->TriggerStageID)
	{
		CLog::Log(TEXT("[StageManager] ========================================"));
		CLog::Log(FString::Printf(TEXT("[StageManager] 스테이지 %d 클리어! 보스 구역 오픈"), StageID));
 		
		CLog::Log(TEXT("[StageManager] 1단계: BossBarrier 열기 명령 전송 중..."));
		BossBarrier->OpenBarrier();
		CLog::Log(TEXT("[StageManager] BossBarrier 열림"));
 		
		CLog::Log(TEXT("[StageManager] 2단계: 이전 스테이지 정리 중..."));
		ClearAllEnemies();
		CLog::Log(TEXT("[StageManager] 이전 스테이지 정리 완료"));
 		
		CLog::Log(TEXT("[StageManager] 보스 스테이지 진입 가능"));
		CLog::Log(TEXT("[StageManager] ========================================"));
	}
	else
	{
		CLog::Log(FString::Printf(TEXT("[StageManager] 다른 스테이지(%d) 클리어 - 보스 배리어 무시"), StageID));
	}
 	
	CLog::Log(TEXT("================================================================="));
}

void ACStageManager::CollectBossBarrier()
{
	if (!GetWorld())
		return;
	
	if (IsValid(BossBarrier))
		return;
	
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACBossStageBarrier::StaticClass(), FoundActors);
	
	if (FoundActors.Num() == 0)
	{
		if (bEnableDebugLogs)
			CLog::Log(TEXT("[StageManager] CollectBossBarrier: 레벨에 BossBarrier가 존재하지 않음"));
		return;
	}
	
	if (FoundActors.Num() > 1)
	{
		CLog::Log(FString::Printf(TEXT("[StageManager] CollectBossBarrier: BossBarrier %d개 발견"), FoundActors.Num()));
	}
	
	for (AActor* Actor : FoundActors)
	{
		if (ACBossStageBarrier* Barrier = Cast<ACBossStageBarrier>(Actor))
		{
			RegisterBossBarrier(Barrier);
			break;
		}
	}
}

void ACStageManager::OnEnemyDied(AActor* DeadActor)
{
	if (!IsValid(DeadActor) || bIsClearingEnemies)
		return;
	if (UWorld* World = GetWorld())
	{
		if (World->bIsTearingDown)
			return;
	}
	else
	{
		return;
	}

	// 삭제 대기중이면 무시
	if (DeadActor->IsPendingKillPending())
		return;
	
	ACEnemyCharacterBase* DeadEnemy = Cast<ACEnemyCharacterBase>(DeadActor);
	int32 FoundStage = -1; // 어느 스테이지의 적인지 찾기
	
	for (const auto& Pair : StageEnemies)
	{
		if (Pair.Value.Contains(DeadEnemy))
		{
			FoundStage = Pair.Key;
			break;
		}
	}

	if (FoundStage == -1)
		return;

	int32 RemainingBefore = GetRemainingEnemies(FoundStage);
	CLog::Log(FString::Printf(TEXT("[StageManager] 적 사망: Stage %d, 남은 적: %d"), FoundStage, RemainingBefore));
	
	// 스테이지 클리어 확인
	CheckStageComplete(FoundStage);
	// 선제적 로딩 체크 (여기서만 해야함)
	CheckPreloadTrigger();
	
	FTimerHandle HideTimer;
	GetWorldTimerManager().SetTimer(
		HideTimer,
		FTimerDelegate::CreateWeakLambda(this, [this, DeadEnemy, FoundStage]()
		{
			if (!IsValid(DeadEnemy))
				return;

			DeadEnemy->SetActorHiddenInGame(true);
			DeadEnemy->SetActorEnableCollision(false);
			DeadEnemy->SetActorTickEnabled(false);
		}),
		DeathHideDelay,
		false);

	int32 Remaining = GetRemainingEnemies(FoundStage);
	if (bEnableDebugLogs)
	{
		CLog::Log(FString::Printf(TEXT("[ACStageManager::OnEnemyDied] 스테이지 %d 적 사망. 남은 적: %d"),
			FoundStage, Remaining));
	}
}

void ACStageManager::CheckPreloadTrigger()
{
	int32 Remaining = GetRemainingEnemies(CurrentStage);

	if (Remaining <= PreloadTriggerCount && Remaining > 0)
	{
		int32 NextStage = CurrentStage + 1;
		
		if (StageSpawnZones.Contains(NextStage) && !PreloadedStages.Contains(NextStage))
		{
			if (SpawningStage == NextStage && bIsPreloading)
				return;
				
			CLog::Log(FString::Printf(TEXT("[ACStageManager::CheckPreloadTrigger] 선제 로딩 (남은 적: %d), 해당 스테이지 %d"), Remaining, NextStage));
				
			if (IsSpawnInProgress())
				QueueSpawnRequest(NextStage, true);
			else
				StartDistributedSpawn(NextStage, true);
		}
	}
}

void ACStageManager::ResetEnemy(ACEnemyCharacterBase* Enemy)
{
	if (!IsValid(Enemy) || Enemy->IsPendingKillPending())
		return;

	Enemy->SetLifeSpan(0.0f);
	Enemy->ResetForRespawn();
	Enemy->ResetToInitialTransform();

	if (UCBaseHealthComponent* HealthComp = Enemy->FindComponentByClass<UCBaseHealthComponent>())
	{
		HealthComp->OnDeath.RemoveAll(this);
		HealthComp->OnDeath.AddDynamic(this, &ACStageManager::OnEnemyDied);
	}

	Enemy->ForceRestartAI();

	Enemy->SetActorHiddenInGame(false);
	Enemy->SetActorTickEnabled(true);

	if (UWorld* World = Enemy->GetWorld())
	{
		TWeakObjectPtr<ACEnemyCharacterBase> WeakEnemy = Enemy;
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakEnemy]()
		{
				if (WeakEnemy.IsValid())
				{
						WeakEnemy->OnRespawned();
				}
	   }));
	}
	else
	{
		Enemy->OnRespawned();
	}
}

bool ACStageManager::IsSpawnInProgress() const
{
	return SpawnTimer.IsValid() && SpawningStage != -1 && CurrentSpawnQueue.Num() > 0;
}

void ACStageManager::QueueSpawnRequest(int32 StageID, bool bIsPreload)
{
	for (FStageSpawnRequest& Request : SpawnRequestQueue)
	{
		if (Request.StageID == StageID)
		{
			if (!bIsPreload && Request.bIsPreload)
			{
				Request.bIsPreload = false;
			}
			return;
		}
	}

	SpawnRequestQueue.Add(FStageSpawnRequest(StageID, bIsPreload));
	
	if (bEnableDebugLogs)
	{
		const FString ModeString = bIsPreload ? TEXT("Preload") : TEXT("Spawn");
		CLog::Log(FString::Printf(TEXT("[ACStageManager::QueueSpawnRequest] Stage %d queued (%s)"), StageID, *ModeString));
	}
	
	if (!IsSpawnInProgress())
		ProcessNextSpawnRequest();
}

void ACStageManager::ResetSpawnState()
{
	if (SpawnTimer.IsValid())
		GetWorldTimerManager().ClearTimer(SpawnTimer);
	
	SpawningStage = -1;
	bIsPreloading = false;
	SpawnProgress = 0;
	SpawnPerFrame = 0;
	CurrentSpawnQueue.Empty();
}

void ACStageManager::ClearAllEnemies()
{
	TGuardValue<bool> GuardClearing(bIsClearingEnemies, true);
	
	int32 TotalCleared = 0;
	
	auto ClearEnemyMap = [this, &TotalCleared](auto& EnemyMap)
	{
		for (auto& Pair : EnemyMap)
		{
			for (TObjectPtr<ACEnemyCharacterBase>& EnemyPtr : Pair.Value)
			{
				ACEnemyCharacterBase* Enemy = EnemyPtr.Get();
				if (!IsValid(Enemy))
					continue;
						
				if (UCBaseHealthComponent* HealthComp = Enemy->FindComponentByClass<UCBaseHealthComponent>())
				{
					HealthComp->OnDeath.RemoveDynamic(this, &ACStageManager::OnEnemyDied);
				}
						
				Enemy->Destroy();
				TotalCleared++;
			}
				
			Pair.Value.Empty();
		}
	};
	
	ClearEnemyMap(StageEnemies);
	ClearEnemyMap(PreloadedEnemies);
	
	StageEnemies.Empty();
	PreloadedEnemies.Empty();
}
