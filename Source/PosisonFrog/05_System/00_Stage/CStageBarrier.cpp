// Fill out your copyright notice in the Description page of Project Settings.

#include "CStageBarrier.h"

#include "99_Util/CLog.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"


// Sets default values
ACStageBarrier::ACStageBarrier()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = OpacityUpdateInterval;

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

// Called when the game starts or when spawned
void ACStageBarrier::BeginPlay()
{
	Super::BeginPlay();
	PrimaryActorTick.TickInterval = OpacityUpdateInterval;

	CLog::Log(TEXT("================================================================="));
	CLog::Log(FString::Printf(TEXT("[Barrier] 섹션 %d 바리게이트 생성 시작"), SectionID));
	CLog::Log(FString::Printf(TEXT("[Barrier] 위치: %s"), *GetActorLocation().ToString()));
	CLog::Log(FString::Printf(TEXT("[Barrier] MinDistance: %.1f, MaxDistance: %.1f, MaxOpacity: %.2f"), 
		MinVisibilityDistance, MaxVisibilityDistance, MaxOpacity));

	bIsOpen = false;
	
	SetActorTickEnabled(true);
	CLog::Log(TEXT("[Barrier] Tick 활성화"));

	if (CollisionBox)
	{
		CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		CLog::Log(TEXT("[Barrier] CollisionBox 충돌 활성화"));
	}

	if (BarrierMesh)
	{
		BarrierMesh->SetVisibility(true);
		CLog::Log(TEXT("[Barrier] BarrierMesh 표시"));
	}

	if (IsValid(BarrierMesh))
	{
		CLog::Log(TEXT("[Barrier] BarrierMesh 유효함"));
		UMaterialInterface* BaseMaterial = BarrierMesh->GetMaterial(0);
		if (IsValid(BaseMaterial))
		{
			CLog::Log(FString::Printf(TEXT("[Barrier] 베이스 머티리얼 발견: %s"), *BaseMaterial->GetName()));
			DynamicMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
			BarrierMesh->SetMaterial(0, DynamicMaterial);
			CLog::Log(TEXT("[Barrier] ✓ 다이나믹 머티리얼 생성 완료"));
			
			// 테스트: 초기값 설정
			DynamicMaterial->SetScalarParameterValue(FName("Opacity"), 0.5f);
			CLog::Log(TEXT("[Barrier] 테스트: Opacity 파라미터를 0.5로 설정"));
		}
		else
		{
			CLog::Log(TEXT("[Barrier] ✗ 경고: BarrierMesh에 머티리얼이 없습니다!"));
			CLog::Log(TEXT("[Barrier] 에디터에서 BarrierMesh에 머티리얼을 할당해주세요."));
		}
	}
	else
	{
		CLog::Log(TEXT("[Barrier] ✗ 경고: BarrierMesh가 nullptr입니다!"));
	}

	CachedPlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	APawn* PlayerPawn = CachedPlayerPawn.Get();
	if (IsValid(PlayerPawn))
	{
		CLog::Log(FString::Printf(TEXT("[Barrier] ✓ 플레이어 발견: %s"), *PlayerPawn->GetName()));
		CLog::Log(FString::Printf(TEXT("[Barrier] 플레이어 위치: %s"), *PlayerPawn->GetActorLocation().ToString()));
		
		FVector BarrierPos = GetActorLocation();
		FVector PlayerPos = PlayerPawn->GetActorLocation();
		FVector2D BarrierPos2D(BarrierPos.X, BarrierPos.Y);
		FVector2D PlayerPos2D(PlayerPos.X, PlayerPos.Y);
		float InitialDistance2D = FVector2D::Distance(BarrierPos2D, PlayerPos2D);
		float ZDiff = FMath::Abs(BarrierPos.Z - PlayerPos.Z);
		
		CLog::Log(FString::Printf(TEXT("[Barrier] 초기 2D 거리: %.1f (Z차이: %.1f)"), InitialDistance2D, ZDiff));
	}
	else
	{
		CLog::Log(TEXT("[Barrier] ✗ 플레이어를 찾을 수 없음!"));
	}

	if (IsValid(DynamicMaterial))
	{
		UpdateOpacityByDistance();
		CLog::Log(TEXT("[Barrier] 초기 투명도 업데이트 완료"));
	}

	CLog::Log(FString::Printf(TEXT("[Barrier] 섹션 %d 바리게이트 생성 완료 (닫힌 상태)"), SectionID));
	CLog::Log(TEXT("================================================================="));
}

void ACStageBarrier::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 장벽이 열려있지 않고, 다이나믹 머티리얼이 있을 때만 업데이트
	if (!bIsOpen && IsValid(DynamicMaterial))
	{
		UpdateOpacityByDistance();
	}
}

void ACStageBarrier::UpdateOpacityByDistance()
{
	APawn* PlayerPawn = CachedPlayerPawn.Get();
	if (!IsValid(PlayerPawn))
	{
		CachedPlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
		PlayerPawn = CachedPlayerPawn.Get();
	}
	if (!IsValid(PlayerPawn))
	{
		static bool bLoggedOnce = false;
		if (!bLoggedOnce)
		{
			CLog::Log(FString::Printf(TEXT("[Barrier %d] 플레이어를 찾을 수 없음!"), SectionID));
			bLoggedOnce = true;
		}
		return;
	}

	FVector BarrierPos = GetActorLocation();
	FVector PlayerPos = PlayerPawn->GetActorLocation();
	
	FVector2D BarrierPos2D(BarrierPos.X, BarrierPos.Y);
	FVector2D PlayerPos2D(PlayerPos.X, PlayerPos.Y);
	float Distance = FVector2D::Distance(BarrierPos2D, PlayerPos2D);

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
		float Alpha = (MaxVisibilityDistance - Distance) / (MaxVisibilityDistance - MinVisibilityDistance);
		Opacity = FMath::Lerp(0.0f, MaxOpacity, Alpha);
	}

	static float LastLogTime = 0.0f;
	float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime - LastLogTime > 5.0f)
	{
		float ZDiff = FMath::Abs(BarrierPos.Z - PlayerPos.Z);
		CLog::Log(FString::Printf(TEXT("[Barrier %d] 2D거리: %.1f (Z차이: %.1f) | Opacity: %.2f | Min: %.1f | Max: %.1f"), 
			SectionID, Distance, ZDiff, Opacity, MinVisibilityDistance, MaxVisibilityDistance));
		CLog::Log(FString::Printf(TEXT("[Barrier %d] 장벽 위치: %s | 플레이어 위치: %s"), 
			SectionID, *BarrierPos.ToString(), *PlayerPos.ToString()));
		LastLogTime = CurrentTime;
	}

	// 머티리얼 파라미터 업데이트 (머티리얼에 "Opacity" 라는 이름의 파라미터가 있어야 함 -> 참고로 S키 눌러서 하면됨.)
	if (IsValid(DynamicMaterial))
	{
		DynamicMaterial->SetScalarParameterValue(FName("Opacity"), Opacity);
		
		float CheckValue = 0.0f;
		if (DynamicMaterial->GetScalarParameterValue(FName("Opacity"), CheckValue))
		{
			static bool bLoggedOnce = false;
			if (!bLoggedOnce)
			{
				CLog::Log(FString::Printf(TEXT("[Barrier %d] Opacity 파라미터 설정 성공: %.2f"), SectionID, CheckValue));
				bLoggedOnce = true;
			}
		}
		else
		{
			static bool bLoggedError = false;
			if (!bLoggedError)
			{
				CLog::Log(FString::Printf(TEXT("[Barrier %d] ✗ 경고: 머티리얼에 'Opacity' 파라미터가 없습니다!"), SectionID));
				CLog::Log(TEXT("[Barrier] 새로운 투명 머티리얼을 만들어서 할당해야 합니다."));
				bLoggedError = true;
			}
		}
	}
}

void ACStageBarrier::PlayOpenEffects()
{
	if (BarrierMesh)
		BarrierMesh->SetVisibility(false);

	// 여기에 사운드/VFX 추가
}

void ACStageBarrier::PlayCloseEffects()
{
	if (BarrierMesh)
	{
		BarrierMesh->SetVisibility(true);
	}
	
	// 여기에 사운드/VFX 추가
}

void ACStageBarrier::FullyDeactivate()
{
	if (!bIsOpen)
	{
		return;
	}

	SetActorTickEnabled(false);

	if (BarrierMesh)
	{
		BarrierMesh->SetVisibility(false);
		BarrierMesh->SetActive(false);
	}

	if (CollisionBox)
	{
		CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CollisionBox->SetActive(false);
	}

	CLog::Log(FString::Printf(TEXT("[ACStageBarrier::FullyDeactivate] 섹션 %d FullyDeactivate"), SectionID));
}

void ACStageBarrier::OpenBarrier()
{
	if (bIsOpen)
	{
		CLog::Log(FString::Printf(TEXT("[ACStageBarrier::OpenBarrier] 섹션 %d 이미 열려있음."), SectionID));
		return;
	}

	bIsOpen = true;
	CLog::Log(FString::Printf(TEXT("[ACStageBarrier::OpenBarrier] 섹션 %d 열리는 중."), SectionID));

	if (CollisionBox)
		CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	PlayOpenEffects();

	OnBarrierOpened.Broadcast(SectionID);

	FTimerHandle DeactivateTimerHandle;
	GetWorldTimerManager().SetTimer(DeactivateTimerHandle, this, &ACStageBarrier::FullyDeactivate, 2.0f, false);
}

void ACStageBarrier::CloseBarrier()
{
	if (!bIsOpen)
	{
		CLog::Log(FString::Printf(TEXT("[ACStageBarrier::CloseBarrier] 섹션 %d 이미 닫혀있음."), SectionID));
		return;
	}

	bIsOpen = false;
	CLog::Log(FString::Printf(TEXT("[ACStageBarrier::CloseBarrier] 섹션 %d 닫는 중."), SectionID));
	PrimaryActorTick.TickInterval = OpacityUpdateInterval;

	if (CollisionBox)
		CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	if (BarrierMesh)
		BarrierMesh->SetVisibility(true);

	PlayCloseEffects();

	if (IsValid(DynamicMaterial))
	{
		UpdateOpacityByDistance();
		CLog::Log(TEXT("[Barrier] CloseBarrier - 즉시 투명도 업데이트 수행"));
	}
}
