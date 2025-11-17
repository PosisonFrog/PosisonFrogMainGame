// Fill out your copyright notice in the Description page of Project Settings.


#include "CBossStageBarrier.h"

#include "CStageManager.h"
#include "99_Util/CLog.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"

ACBossStageBarrier::ACBossStageBarrier()
{
	PrimaryActorTick.bCanEverTick = true;

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
	CLog::Log(FString::Printf(TEXT("[BossBarrier] 위치: %s"), *GetActorLocation().ToString()));
	CLog::Log(FString::Printf(TEXT("[BossBarrier] TriggerStageID: %d"), TriggerStageID));
	CLog::Log(FString::Printf(TEXT("[BossBarrier] MinDistance: %.1f, MaxDistance: %.1f, MaxOpacity: %.2f"), 
		MinVisibilityDistance, MaxVisibilityDistance, MaxOpacity));
	
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
	
	// 초기 상태: 장벽 닫힘 (bIsOpen = false)
	bIsOpen = false;
	bIsBossBattleActive = false;
	
	// Tick 활성화 (거리 기반 투명도 조절을 위해 필수!)
	SetActorTickEnabled(true);
	CLog::Log(TEXT("[BossBarrier] Tick 활성화"));

	// 콜리전 활성화
	if (CollisionBox)
	{
		CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		CLog::Log(TEXT("[BossBarrier] CollisionBox 충돌 활성화"));
	}

	// 메시 표시
	if (BarrierMesh)
	{
		BarrierMesh->SetVisibility(true);
		CLog::Log(TEXT("[BossBarrier] BarrierMesh 표시"));
	}

	// 다이나믹 머티리얼 생성
	if (IsValid(BarrierMesh))
	{
		CLog::Log(TEXT("[BossBarrier] BarrierMesh 유효함"));
		UMaterialInterface* BaseMaterial = BarrierMesh->GetMaterial(0);
		if (IsValid(BaseMaterial))
		{
			CLog::Log(FString::Printf(TEXT("[BossBarrier] 베이스 머티리얼 발견: %s"), *BaseMaterial->GetName()));
			DynamicMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
			BarrierMesh->SetMaterial(0, DynamicMaterial);
			CLog::Log(TEXT("[BossBarrier] ✓ 다이나믹 머티리얼 생성 완료"));
			
			// 테스트: 초기값 설정
			DynamicMaterial->SetScalarParameterValue(FName("Opacity"), 0.5f);
			CLog::Log(TEXT("[BossBarrier] 테스트: Opacity 파라미터를 0.5로 설정"));
		}
		else
		{
			CLog::Log(TEXT("[BossBarrier] ✗ 경고: BarrierMesh에 머티리얼이 없습니다!"));
			CLog::Log(TEXT("[BossBarrier] 에디터에서 BarrierMesh에 머티리얼을 할당해주세요."));
		}
	}
	else
	{
		CLog::Log(TEXT("[BossBarrier] ✗ 경고: BarrierMesh가 nullptr입니다!"));
	}

	// 플레이어 찾기 테스트
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (IsValid(PlayerPawn))
	{
		CLog::Log(FString::Printf(TEXT("[BossBarrier] ✓ 플레이어 발견: %s"), *PlayerPawn->GetName()));
		CLog::Log(FString::Printf(TEXT("[BossBarrier] 플레이어 위치: %s"), *PlayerPawn->GetActorLocation().ToString()));
		
		// 2D 거리 계산
		FVector BarrierPos = GetActorLocation();
		FVector PlayerPos = PlayerPawn->GetActorLocation();
		FVector2D BarrierPos2D(BarrierPos.X, BarrierPos.Y);
		FVector2D PlayerPos2D(PlayerPos.X, PlayerPos.Y);
		float InitialDistance2D = FVector2D::Distance(BarrierPos2D, PlayerPos2D);
		float ZDiff = FMath::Abs(BarrierPos.Z - PlayerPos.Z);
		
		CLog::Log(FString::Printf(TEXT("[BossBarrier] 초기 2D 거리: %.1f (Z차이: %.1f)"), InitialDistance2D, ZDiff));
	}
	else
	{
		CLog::Log(TEXT("[BossBarrier] ✗ 플레이어를 찾을 수 없음!"));
	}

	// 초기 투명도 업데이트
	if (IsValid(DynamicMaterial))
	{
		UpdateOpacityByDistance();
		CLog::Log(TEXT("[BossBarrier] 초기 투명도 업데이트 완료"));
	}
	
	CLog::Log(TEXT("[BossBarrier] 초기 상태: 장벽 닫힘"));
	CLog::Log(TEXT("================================================================="));
}

void ACBossStageBarrier::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void ACBossStageBarrier::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 장벽이 열려있지 않고, 다이나믹 머티리얼이 있을 때만 업데이트
	if (!bIsOpen && IsValid(DynamicMaterial))
	{
		UpdateOpacityByDistance();
	}
}

void ACBossStageBarrier::UpdateOpacityByDistance()
{
	// 플레이어 캐릭터 찾기
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!IsValid(PlayerPawn))
	{
		static bool bLoggedOnce = false;
		if (!bLoggedOnce)
		{
			CLog::Log(TEXT("[BossBarrier] 플레이어를 찾을 수 없음!"));
			bLoggedOnce = true;
		}
		return;
	}

	// 플레이어와의 2D 거리 계산 (Z축 무시)
	FVector BarrierPos = GetActorLocation();
	FVector PlayerPos = PlayerPawn->GetActorLocation();
	
	// Z축을 무시한 2D 거리
	FVector2D BarrierPos2D(BarrierPos.X, BarrierPos.Y);
	FVector2D PlayerPos2D(PlayerPos.X, PlayerPos.Y);
	float Distance = FVector2D::Distance(BarrierPos2D, PlayerPos2D);

	// 거리에 따른 투명도 계산
	float Opacity = 0.0f;

	if (Distance <= MinVisibilityDistance)
	{
		// 최소 거리 이하면 최대 투명도
		Opacity = MaxOpacity;
	}
	else if (Distance >= MaxVisibilityDistance)
	{
		// 최대 거리 이상이면 완전 투명
		Opacity = 0.0f;
	}
	else
	{
		// 그 사이는 선형 보간
		float Alpha = (MaxVisibilityDistance - Distance) / (MaxVisibilityDistance - MinVisibilityDistance);
		Opacity = FMath::Lerp(0.0f, MaxOpacity, Alpha);
	}

	// 디버그 로그 (5초마다 한 번씩만 출력)
	static float LastLogTime = 0.0f;
	float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime - LastLogTime > 5.0f)
	{
		float ZDiff = FMath::Abs(BarrierPos.Z - PlayerPos.Z);
		CLog::Log(FString::Printf(TEXT("[BossBarrier] 2D거리: %.1f (Z차이: %.1f) | Opacity: %.2f | Min: %.1f | Max: %.1f"), 
			Distance, ZDiff, Opacity, MinVisibilityDistance, MaxVisibilityDistance));
		CLog::Log(FString::Printf(TEXT("[BossBarrier] 장벽 위치: %s | 플레이어 위치: %s"), 
			*BarrierPos.ToString(), *PlayerPos.ToString()));
		LastLogTime = CurrentTime;
	}

	// 머티리얼 파라미터 업데이트 (머티리얼에 "Opacity" 파라미터가 있어야 함)
	if (IsValid(DynamicMaterial))
	{
		DynamicMaterial->SetScalarParameterValue(FName("Opacity"), Opacity);
	}
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

	// Tick 비활성화
	SetActorTickEnabled(false);

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

	// Tick 활성화 (거리에 따른 투명도 조절을 위해)
	SetActorTickEnabled(true);

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

	// 닫힐 때 즉시 투명도 업데이트
	if (IsValid(DynamicMaterial))
	{
		UpdateOpacityByDistance();
		CLog::Log(TEXT("[BossBarrier] CloseBarrier - 즉시 투명도 업데이트 수행"));
	}

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
	// Tick 비활성화
	SetActorTickEnabled(false);
	
	if (IsValid(BarrierMesh))
		BarrierMesh->SetVisibility(false);

	CLog::Log(TEXT("[BossBarrier] 장벽 완전 비활성화"));
}