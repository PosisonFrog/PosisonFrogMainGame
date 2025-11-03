// Fill out your copyright notice in the Description page of Project Settings.


#include "CStageManager.h"

#include "AIController.h"
#include "BrainComponent.h"
#include "CCheckPoint.h"
#include "CEnemySpawnZone.h"
#include "CStageBarrier.h"
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

	if (bEnableDebugLogs)
	{
		CLog::Log(FString::Printf(TEXT("[CheckStageComplete] Stage %d - 남은 적: %d"), 
			StageID, Remaining));
	}

	if (Remaining == 0)
	{
		OnStageComplete(StageID);
	}
}

void ACStageManager::PrepareForRespawn(int32 TargetStageID)
{
	CLog::Log(FString::Printf(TEXT("[PrepareForRespawn] Stage %d로 리스폰 준비 시작"), TargetStageID));
	
	GetWorldTimerManager().ClearAllTimersForObject(this);
	ResetSpawnState();

	for (auto StageIterator = StageEnemies.CreateIterator(); StageIterator; ++StageIterator)
	{
		const int32 StageID = StageIterator.Key();
		if (StageID >= TargetStageID)
			continue;

		for (ACEnemyCharacterBase* Enemy : StageIterator.Value())
		{
			if (IsValid(Enemy))
			{
				// 헬스 컴포넌트 이벤트 정리
				if (UCBaseHealthComponent* HealthComp = Enemy->FindComponentByClass<UCBaseHealthComponent>())
				{
					HealthComp->OnDeath.RemoveAll(this);
				}
				Enemy->Destroy();
			}
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
			{
				if (UCBaseHealthComponent* HealthComp = Enemy->FindComponentByClass<UCBaseHealthComponent>())
				{
					HealthComp->OnDeath.RemoveAll(this);
				}
				Enemy->Destroy();
			}
		}

		PreloadIterator.RemoveCurrent();
	}

	for (auto FlagIterator = PreloadedStages.CreateIterator(); FlagIterator; ++FlagIterator)
	{
		if (*FlagIterator < TargetStageID)
			FlagIterator.RemoveCurrent();
	}

	for (auto DeadIterator = StageDeadEnemies.CreateIterator(); DeadIterator; ++DeadIterator)
	{
		if (DeadIterator.Key() < TargetStageID)
			DeadIterator.RemoveCurrent();
	}

	bool bReactivated = false;

	if (TArray<TObjectPtr<ACEnemyCharacterBase>>* ActiveArray = StageEnemies.Find(TargetStageID))
	{
		if (ActiveArray->Num() > 0)
		{
			// 모든 적(죽은 적 포함) 리셋
			for (ACEnemyCharacterBase* Enemy : *ActiveArray)
			{
				if (IsValid(Enemy))
					ResetEnemy(Enemy);
			}
			
			// 죽은 적 기록 초기화
			StageDeadEnemies.FindOrAdd(TargetStageID).Empty();
			
			bReactivated = true;
			CLog::Log(FString::Printf(TEXT("[PrepareForRespawn] Stage %d 적들 리셋 완료 (%d마리)"), 
				TargetStageID, ActiveArray->Num()));
		}
	}

	if (!bReactivated && PreloadedEnemies.Contains(TargetStageID))
	{
		ActivatePreloadedStage(TargetStageID);
		bReactivated = true;
	}

	if (!bReactivated)
	{
		CLog::Log(FString::Printf(TEXT("[PrepareForRespawn] Stage %d 새로 스폰"), TargetStageID));
		StartStageSpawn(TargetStageID);
	}
	
	CurrentStage = TargetStageID;

	for (auto ClearIterator = ClearedStages.CreateIterator(); ClearIterator; ++ClearIterator)
	{
		if (*ClearIterator >= TargetStageID)
			ClearIterator.RemoveCurrent();
	}
	
	CLog::Log(FString::Printf(TEXT("[PrepareForRespawn] Stage %d 리스폰 준비 완료!"), TargetStageID));
}

/*void ACStageManager::RespawnStage(int32 StageID)
{
	CLog::Log(FString::Printf(TEXT("[ACStageManager::RespawnStage] Stage %d 리스폰 요청 받음"), StageID));

	// 해당 스테이지의 기본 적 제거
	if (auto* Enemies = StageEnemies.Find(StageID))
	{
		for (ACEnemyCharacterBase* Enemy : *Enemies)
		{
			if (IsValid(Enemy))
			{
				if (UCBaseHealthComponent* HealthComp = Enemy->FindComponentByClass<UCBaseHealthComponent>())
				{
					HealthComp->OnDeath.RemoveAll(this);
				}
				
				Enemy->Destroy();
			}
		}
		Enemies->Empty();
	}

	// 선제 로딩된 적도 제거
	if (auto* PreloadedArray = PreloadedEnemies.Find(StageID))
	{
		for (ACEnemyCharacterBase* Enemy : *PreloadedArray)
		{
			if (IsValid(Enemy))
			{
				Enemy->Destroy();
			}
		}
		PreloadedEnemies.Remove(StageID);
	}

	PreloadedStages.Remove(StageID);
	StartStageSpawn(StageID);
}*/

int32 ACStageManager::GetRemainingEnemies(int32 StageID) const
{
	const TArray<TObjectPtr<ACEnemyCharacterBase>>* All = StageEnemies.Find(StageID);
	if (!All)
		return 0;

	const TSet<TWeakObjectPtr<ACEnemyCharacterBase>>* Dead = StageDeadEnemies.Find(StageID);

	int32 Count = 0;
	int32 TotalEnemies = 0;
	int32 InvalidEnemies = 0;
	int32 DeadEnemies = 0;
	
	for (const TObjectPtr<ACEnemyCharacterBase>& Ptr : *All)
	{
		TotalEnemies++;
		ACEnemyCharacterBase* Enemy = Ptr.Get();
		if (!IsValid(Enemy))
		{
			InvalidEnemies++;
			continue;
		}
			
		// TWeakObjectPtr와 직접 비교
		bool bIsDead = false;
		if (Dead)
		{
			for (const TWeakObjectPtr<ACEnemyCharacterBase>& WeakPtr : *Dead)
			{
				if (WeakPtr.Get() == Enemy)
				{
					bIsDead = true;
					DeadEnemies++;
					break;
				}
			}
		}
		
		if (!bIsDead)
			++Count;
	}

	if (bEnableDebugLogs && DeadEnemies > 0)
	{
		CLog::Log(FString::Printf(TEXT("[GetRemainingEnemies] Stage %d - Total: %d, Invalid: %d, Dead: %d, Remaining: %d"),
			StageID, TotalEnemies, InvalidEnemies, DeadEnemies, Count));
	}

	return Count;
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
				if (AAIController* AI = Cast<AAIController>(Enemy->GetController()))
				{
					AI->StopMovement();
					AI->ClearFocus(EAIFocusPriority::Gameplay);
				}
				
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

		// 활성화
		Enemy->SetActorHiddenInGame(false);
		Enemy->SetActorEnableCollision(true);
		Enemy->SetActorTickEnabled(true);

		if (AAIController* AI = Cast<AAIController>(Enemy->GetController()))
		{
			if (UBrainComponent* Brain = AI->GetBrainComponent())
			{
				Brain->ResumeLogic(TEXT("Activated"));
			}
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
	StageDeadEnemies.FindOrAdd(StageID).Empty();
	CurrentStage = StageID;
}


void ACStageManager::OnStageComplete(int32 StageID)
{
	if (ClearedStages.Contains(StageID))
		return;

	ClearedStages.Add(StageID);

	CLog::Log(FString::Printf(TEXT("[OnStageComplete] ===== 스테이지 %d 클리어 ====="), StageID));

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
		CLog::Log(FString::Printf(TEXT("[OnStageComplete] 다음 스테이지 (%d) 스폰 시작"), NextStage));
		StartStageSpawn(NextStage);
	}
	else
	{
		CLog::Log(FString::Printf(TEXT("[OnStageComplete] 다음 스테이지 (%d) 없음 - 게임 완료"), NextStage));
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
	
	ACEnemyCharacterBase* DeadEnemy = Cast<ACEnemyCharacterBase>(DeadActor);
	int32 FoundStage = -1; // 어느 스테이지의 적인지 찾기
	
	for (auto& Pair : StageEnemies)
	{
		if (Pair.Value.Contains(DeadEnemy))
		{
			FoundStage = Pair.Key;
			// TWeakObjectPtr로 명시적 변환하여 추가
			StageDeadEnemies.FindOrAdd(FoundStage).Add(TWeakObjectPtr<ACEnemyCharacterBase>(DeadEnemy));
			
			if (bEnableDebugLogs)
			{
				int32 DeadCount = StageDeadEnemies.FindOrAdd(FoundStage).Num();
				CLog::Log(FString::Printf(TEXT("[OnEnemyDied] Stage %d - 죽은 적 추가됨. 총 죽은 적: %d"), 
					FoundStage, DeadCount));
			}
			break;
		}
	}

	if (FoundStage == -1)
		return;

	// 스테이지 클리어 확인
	CheckStageComplete(FoundStage);
	// 선제적 로딩 체크 (여기서만 해야함)
	CheckPreloadTrigger();
	
	// TWeakObjectPtr로 안전하게 캡처
	TWeakObjectPtr<ACEnemyCharacterBase> WeakEnemy(DeadEnemy);
	
	FTimerHandle HideTimer;
	GetWorldTimerManager().SetTimer(
		HideTimer,
		FTimerDelegate::CreateWeakLambda(this, [this, WeakEnemy, FoundStage]()
		{
			ACEnemyCharacterBase* Enemy = WeakEnemy.Get();
			if (!IsValid(Enemy))
				return;

			Enemy->SetActorHiddenInGame(true);
			Enemy->SetActorEnableCollision(false);
			Enemy->SetActorTickEnabled(false);
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
				
			CLog::Log(FString::Printf(TEXT("[ACStageManager::CheckPreloadTrigger] 선제 로딩 (남은 적: %d) → 스테이지 %d"), Remaining, NextStage));
				
			if (IsSpawnInProgress())
				QueueSpawnRequest(NextStage, true);
			else
				StartDistributedSpawn(NextStage, true);
		}
	}
}

void ACStageManager::ResetEnemy(ACEnemyCharacterBase* Enemy)
{
	if (!IsValid(Enemy))
		return;

	Enemy->ResetToInitialTransform();

	if (UCBaseHealthComponent* HealthComp = Enemy->FindComponentByClass<UCBaseHealthComponent>())
	{
		HealthComp->ResetHealth();
		HealthComp->OnDeath.RemoveAll(this);
		HealthComp->OnDeath.AddDynamic(this, &ACStageManager::OnEnemyDied);
	}

	if (AAIController* AI = Cast<AAIController>(Enemy->GetController()))
	{
		AI->StopMovement();
		AI->ClearFocus(EAIFocusPriority::Gameplay);

		if (UBrainComponent* BrainComp = AI->FindComponentByClass<UBrainComponent>())
			BrainComp->ResumeLogic(TEXT("Reset"));
	}

	if (USkeletalMeshComponent* Mesh = Enemy->GetMesh())
	{
		if (UAnimInstance* AnimInst = Mesh->GetAnimInstance())
		{
			AnimInst->Montage_Stop(0.0f);
		}
	}

	Enemy->SetActorEnableCollision(true);
	Enemy->SetCanBeDamaged(true);
	Enemy->SetActorHiddenInGame(false);
	Enemy->SetActorTickEnabled(true);
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
	
	// 월드가 정리되는 중이면 안전하게 종료
	UWorld* World = GetWorld();
	if (!World || World->bIsTearingDown)
	{
		StageEnemies.Empty();
		PreloadedEnemies.Empty();
		StageDeadEnemies.Empty();
		return;
	}
	
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
	StageDeadEnemies.Empty();
}
