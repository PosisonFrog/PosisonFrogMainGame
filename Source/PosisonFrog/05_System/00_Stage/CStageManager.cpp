// Fill out your copyright notice in the Description page of Project Settings.


#include "CStageManager.h"

#include <utility>

#include "AIController.h"
#include "BrainComponent.h"
#include "CCheckPoint.h"
#include "CEnemySpawnZone.h"
#include "CStageBarrier.h"
#include "00_Character/CMainGameModeBase.h"
#include "00_Character/00_Player/CPlayerCharacter.h"
#include "00_Character/01_Enemy/CEnemyCharacterBase.h"
#include "00_Character/02_Component/CBaseHealthComponent.h"
#include "99_Util/CLog.h"
#include "Kismet/GameplayStatics.h"


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

	if (StartStage > 0)
	{
		StartStageSpawn(StartStage);
	}
}

void ACStageManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (SpawnTimer.IsValid())
		GetWorldTimerManager().ClearTimer(SpawnTimer);

	CleanupDelegates();
	ClearAllEnemies();
	
	Super::EndPlay(EndPlayReason);
}

void ACStageManager::StartStageSpawn(int32 StageID)
{
	TArray<TObjectPtr<ACEnemySpawnZone>>* SpawnZones = StageSpawnZones.Find(StageID);
	if (!SpawnZones || SpawnZones->Num() == 0)
	{
		CLog::Log(FString::Printf(TEXT("[ACStageManager::StartStageSpawn] 스테이지에 스폰 포인트가 없음 (StageID : %d)"), StageID));
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

void ACStageManager::RespawnStage(int32 StageID)
{
	CLog::Log(FString::Printf(TEXT("[ACStageManager::RespawnStage] Stage %d 리스폰 요청 받음"), StageID));

	if (auto* Enemies = StageEnemies.Find(StageID))
	{
		int32 ClearedCount = 0;
		for (ACEnemyCharacterBase* Enemy : *Enemies)
		{
			if (IsValid(Enemy))
			{
				Enemy->Destroy();
				ClearedCount++;
			}
		}
		Enemies->Empty();
	}
	
	StartStageSpawn(StageID);
}

int32 ACStageManager::GetRemainingEnemies(int32 StageID) const
{
	if (const TArray<TObjectPtr<ACEnemyCharacterBase>> * Enemies = StageEnemies.Find(StageID))
	{
		int32 ValidCount = 0;

		for (const TObjectPtr<ACEnemyCharacterBase>& Enemy : *Enemies)
		{
			if (IsValid(Enemy.Get()))
				ValidCount++;
		}

		return ValidCount;
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

		int32 StageID = Zone->SectionID;
		StageSpawnZones.FindOrAdd(StageID).Add(Zone);
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
	TArray<TObjectPtr<ACEnemySpawnZone>>* SpawnZones = StageSpawnZones.Find(StageID);
	if (!SpawnZones || SpawnZones->Num() == 0)
		return;

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
		if (SpawnTimer.IsValid())
			GetWorldTimerManager().ClearTimer(SpawnTimer);
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
		SpawnParams.SpawnCollisionHandlingOverride = 
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		// SpawnInfo에서 Transform과 EnemyClass 모두 사용
		ACEnemyCharacterBase* Enemy = GetWorld()->SpawnActor<ACEnemyCharacterBase>(
			SpawnInfo.EnemyClass,      // ← 각 적마다 다른 타입!
			SpawnInfo.Transform,
			SpawnParams);

		if (Enemy)
		{
			if (bIsPreloading)
			{
				// 비활성화 (선제 로딩)
				Enemy->SetActorHiddenInGame(true);
				Enemy->SetActorEnableCollision(false);
				Enemy->SetActorTickEnabled(false);

				if (AAIController* AI = Cast<AAIController>(Enemy->GetController()))
				{
					AI->StopMovement();
					AI->GetBrainComponent()->PauseLogic(TEXT("Preload"));
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
		if (SpawnTimer.IsValid())
			GetWorldTimerManager().ClearTimer(SpawnTimer);

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

		// 상태 초기화
		SpawningStage = -1;
		bIsPreloading = false;
		SpawnProgress = 0;
		CurrentSpawnQueue.Empty();
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

		// 활성화
		Enemy->SetActorHiddenInGame(false);
		Enemy->SetActorEnableCollision(true);
		Enemy->SetActorTickEnabled(true);

		if (AAIController* AI = Cast<AAIController>(Enemy->GetController()))
		{
			AI->GetBrainComponent()->ResumeLogic(TEXT("Activated"));
		}

		if (UCBaseHealthComponent* HealthComp = Enemy->FindComponentByClass<UCBaseHealthComponent>())
		{
			HealthComp->OnDeath.AddDynamic(this, &ACStageManager::OnEnemyDied);
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
	if (ClearedStages.Contains(StageID))
		return;

	ClearedStages.Add(StageID);

	ActivateStageCheckPoint(StageID);
	
	OpenStageBarrier(StageID);
	OnStageCleared.Broadcast(StageID);

	// 다음 스테이지가 이미 선제 로딩되었으면
	int32 NextStage = StageID + 1;
	if (PreloadedStages.Contains(NextStage))
	{
		CLog::Log(FString::Printf(TEXT("[ACStageManager::OnStageComplete] 다음 스테이지 (%d) 이미 로딩되어 있음 준비 완료"), NextStage));
		ActivatePreloadedStage(NextStage);
	}
	else if (StageSpawnZones.Contains(NextStage))
	{
		StartStageSpawn(NextStage);
	}
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

void ACStageManager::OnEnemyDied(AActor* DeadActor)
{
	ACEnemyCharacterBase* DeadEnemy = Cast<ACEnemyCharacterBase>(DeadActor);
	if (!IsValid(DeadActor))
		return;

	// 어느 스테이지의 적인지 찾기
	int32 FoundStage = -1;
	for (auto& Pair : StageEnemies)
	{
		if (Pair.Value.Contains(DeadEnemy))
		{
			FoundStage = Pair.Key;
			Pair.Value.Remove(DeadEnemy);
			break;
		}
	}

	if (FoundStage == -1)
		return;

	int32 Remaining = GetRemainingEnemies(FoundStage);

	if (bEnableDebugLogs)
	{
		CLog::Log(FString::Printf(TEXT("[ACStageManager::OnEnemyDied] 스테이지 %d 적 사망. 남은 적: %d"),
			FoundStage, Remaining));
	}

	// 스테이지 클리어 확인
	CheckStageComplete(FoundStage);

	// 선제적 로딩 체크 (여기서만 해야함)
	CheckPreloadTrigger();
}

void ACStageManager::CheckPreloadTrigger()
{
	int32 Remaining = GetRemainingEnemies(CurrentStage);

	if (Remaining <= PreloadTriggerCount && Remaining > 0)
	{
		int32 NextStage = CurrentStage + 1;

		if (StageSpawnZones.Contains(NextStage) && !PreloadedStages.Contains(NextStage) && SpawningStage != NextStage)
		{
			CLog::Log(FString::Printf(TEXT("[ACStageManager::CheckPreloadTrigger] 선제 로딩 (남은 적: %d) → 스테이지 %d"), Remaining, NextStage));

			// 선제적 로딩 시작
			StartDistributedSpawn(NextStage, true);
		}
	}
}

void ACStageManager::CleanupDelegates()
{
	for (auto& Pair : PreloadedEnemies)
	{
		for (ACEnemyCharacterBase* Enemy : Pair.Value)
		{
			if (IsValid(Enemy))
			{
				Enemy->Destroy();
			}
		}
	}
	PreloadedEnemies.Empty();
}

void ACStageManager::ClearAllEnemies()
{
	int32 TotalCleared = 0;

	for (auto& Pair : StageEnemies)
	{
		for (ACEnemyCharacterBase* Enemy : Pair.Value)
		{
			if (IsValid(Enemy))
			{
				Enemy->Destroy();
				TotalCleared++;
			}
		}

		Pair.Value.Empty();
	}

	for (auto& Pair : PreloadedEnemies)
	{
		for (ACEnemyCharacterBase* Enemy : Pair.Value)
		{
			if (IsValid(Enemy))
			{
				Enemy->Destroy();
				TotalCleared++;
			}
		}

		Pair.Value.Empty();
	}
	PreloadedEnemies.Empty();
}
