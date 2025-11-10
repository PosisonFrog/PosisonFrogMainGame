// Fill out your copyright notice in the Description page of Project Settings.


#include "CBossStageBarrier.h"

#include "CStageManager.h"
#include "99_Util/CLog.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"

ACBossStageBarrier::ACBossStageBarrier()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	CollisionBox->SetupAttachment(RootComponent);
	CollisionBox->SetBoxExtent(FVector(100.0f, 500.0f, 300.0f));
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionBox->SetCollisionObjectType(ECC_WorldStatic);
	CollisionBox->SetCollisionResponseToAllChannels(ECR_Block);

	BarrierMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BarrierMesh"));
	BarrierMesh->SetupAttachment(CollisionBox);
	BarrierMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ACBossStageBarrier::BeginPlay()
{
	Super::BeginPlay();
	
	CLog::Log(TEXT("================================================================="));
	CLog::Log(TEXT("[BossBarrier] BeginPlay"));
	CLog::Log(FString::Printf(TEXT("[BossBarrier] 액터 이름: %s"), *GetName()));
	CLog::Log(FString::Printf(TEXT("[BossBarrier] TriggerStageID: %d"), TriggerStageID));
	
	// StageManager 찾기 및 등록
	CLog::Log(TEXT("[BossBarrier] StageManager 검색"));
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACStageManager::StaticClass(), FoundActors);
	
	if (FoundActors.Num() == 0)
	{
		CLog::Log(TEXT("[BossBarrier] StageManager를 찾을 수 없음"));
		CLog::Log(TEXT("[BossBarrier] 레벨에 ACStageManager가 배치되어 있는지 확인 필요"));
		CLog::Log(TEXT("================================================================="));
		return;
	}
	
	if (FoundActors.Num() > 1)
		CLog::Log(FString::Printf(TEXT("[BossBarrier] StageManager가 %d개 발견됨 - 첫 번째 사용"), FoundActors.Num()));
	
	ACStageManager* StageManager = Cast<ACStageManager>(FoundActors[0]);
	if (!IsValid(StageManager))
	{
		CLog::Log(TEXT("[BossBarrier] StageManager 캐스팅 실패!"));
		CLog::Log(TEXT("================================================================="));
		return;
	}
	
	CLog::Log(FString::Printf(TEXT("[BossBarrier] StageManager 발견: %s"), *StageManager->GetName()));
	CLog::Log(TEXT("[BossBarrier] StageManager에 등록 요청"));
	
	StageManager->RegisterBossBarrier(this);
	
	CLog::Log(TEXT("[BossBarrier] StageManager 등록 완료"));
	CLog::Log(TEXT("[BossBarrier] 이제 StageManager가 이 배리어를 직접 제어합니다"));
	
	CloseBarrier();
	
	CLog::Log(TEXT("[BossBarrier] 초기 상태: 장벽 닫힘"));
	CLog::Log(TEXT("================================================================="));
}

void ACBossStageBarrier::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

// ────────────────────────────────────────────────────────────────────────────
// 외부 명령 인터페이스
// ────────────────────────────────────────────────────────────────────────────

void ACBossStageBarrier::OnBossBattleStart()
{
	CLog::Log(TEXT("[BossBarrier] 보스 전투 시작 명령 받음"));
	
	if (bIsBossBattleActive)
	{
		CLog::Log(TEXT("[BossBarrier] 보스 전투 이미 활성화됨"));
		return;
	}

	bIsBossBattleActive = true;
	CloseBarrier();

	CLog::Log(TEXT("[BossBarrier] 보스 전투 시작 - 장벽 닫힘"));
}

void ACBossStageBarrier::OnBossBattleEnd()
{
	CLog::Log(TEXT("[BossBarrier] 보스 사망 명령 받음"));
	
	bIsBossBattleActive = false;
	OpenBarrier();
	
	CLog::Log(TEXT("[BossBarrier] 장벽 열림 - 보스 구역 탈출 가능"));
}

// ────────────────────────────────────────────────────────────────────────────
// 장벽 제어
// ────────────────────────────────────────────────────────────────────────────

void ACBossStageBarrier::OpenBarrier()
{
	CLog::Log(TEXT("[BossBarrier] OpenBarrier 호출됨"));
	CLog::Log(FString::Printf(TEXT("[BossBarrier] 현재 bIsOpen 상태: %s"), bIsOpen ? TEXT("true") : TEXT("false")));
	
	if (bIsOpen)
	{
		CLog::Log(TEXT("[BossBarrier] 이미 열려있음 - 무시"));
		return;
	}
	
	bIsOpen = true;
	CLog::Log(TEXT("[BossBarrier] bIsOpen = true 설정됨"));

	if (IsValid(CollisionBox))
	{
		CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CLog::Log(TEXT("[BossBarrier] CollisionBox 충돌 비활성화"));
	}
	else
	{
		CLog::Log(TEXT("[BossBarrier] CollisionBox가 nullptr!"));
	}

	PlayOpenEffects();

	OnBarrierOpened.Broadcast();

	GetWorldTimerManager().SetTimer(
		DeactivateTimer,
		this,
		&ACBossStageBarrier::FullyDeactivate,
		DeactivateDelay,
		false
	);

	CLog::Log(TEXT("[BossBarrier] 장벽 개방 완료"));
}

void ACBossStageBarrier::CloseBarrier()
{
	CLog::Log(TEXT("[BossBarrier] CloseBarrier 호출됨"));
	CLog::Log(FString::Printf(TEXT("[BossBarrier] 현재 bIsOpen 상태: %s"), bIsOpen ? TEXT("true") : TEXT("false")));
	
	if (!bIsOpen && IsValid(CollisionBox) && CollisionBox->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
	{
		CLog::Log(TEXT("[BossBarrier] 이미 닫혀있음 - 무시"));
		return;
	}

	bIsOpen = false;
	CLog::Log(TEXT("[BossBarrier] bIsOpen = false 설정됨"));

	GetWorldTimerManager().ClearTimer(DeactivateTimer);

	if (IsValid(CollisionBox))
	{
		CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		CLog::Log(TEXT("[BossBarrier] CollisionBox 충돌 활성화"));
	}
	else
	{
		CLog::Log(TEXT("[BossBarrier] CollisionBox가 nullptr!"));
	}

	if (IsValid(BarrierMesh))
	{
		BarrierMesh->SetVisibility(true);
		CLog::Log(TEXT("[BossBarrier] BarrierMesh 표시"));
	}

	PlayCloseEffects();

	OnBarrierClosed.Broadcast();

	CLog::Log(TEXT("[BossBarrier] 장벽 닫힘"));
}

void ACBossStageBarrier::PlayOpenEffects()
{
	if (IsValid(BarrierMesh))
	{
		BarrierMesh->SetVisibility(false);
		CLog::Log(TEXT("[BossBarrier] BarrierMesh 숨김"));
	}
}

void ACBossStageBarrier::PlayCloseEffects()
{
	if (IsValid(BarrierMesh))
	{
		BarrierMesh->SetVisibility(true);
		CLog::Log(TEXT("[BossBarrier] BarrierMesh 표시"));
	}
}

void ACBossStageBarrier::FullyDeactivate()
{
	if (IsValid(BarrierMesh))
		BarrierMesh->SetVisibility(false);

	CLog::Log(TEXT("[BossBarrier] 장벽 완전 비활성화"));
}
