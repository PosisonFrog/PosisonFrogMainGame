// Fill out your copyright notice in the Description page of Project Settings.


#include "CEnemySpawnZone.h"

#include "00_Character/01_Enemy/CEnemyCharacterBase.h"
#include "99_Util/CLog.h"
#include "Components/SphereComponent.h"


ACEnemySpawnZone::ACEnemySpawnZone()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	SpawnAreaSphere = CreateDefaultSubobject<USphereComponent>(TEXT("SpawnArea"));
	SpawnAreaSphere->SetupAttachment(RootComponent);
	SpawnAreaSphere->SetSphereRadius(SpawnRadius);
	SpawnAreaSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpawnAreaSphere->SetHiddenInGame(true);

#if WITH_EDITORONLY_DATA
	SpawnAreaSphere->bVisualizeComponent = true;
#endif
}

void ACEnemySpawnZone::BeginPlay()
{
	Super::BeginPlay();

	if (EnemyTypes.Num() == 0)
	{
		CLog::Log(TEXT("[ACEnemySpawnZone::BeginPlay] 적 타입 세팅이 안되어 있음"));
	}
}

#if WITH_EDITOR
void ACEnemySpawnZone::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (SpawnAreaSphere)
	{
		SpawnAreaSphere->SetSphereRadius(SpawnRadius);
	}
}
#endif


// ──────────── 스폰 로직 ────────────
int32 ACEnemySpawnZone::GetTotalEnemyCount() const
{
	int32 Total = 0;
	for (const FEnemySpawnInfo& Info : EnemyTypes)
	{
		if (Info.EnemyClass)
			Total += Info.Count;
	}

	return Total;
}

TArray<FSpawnTransformInfo> ACEnemySpawnZone::GenerateSpawnTransforms()
{
	TArray<FSpawnTransformInfo> Results;

	int32 TotalCount = GetTotalEnemyCount();
	if (TotalCount == 0)
	{
		CLog::Log(TEXT("[ACEnemySpawnZone::GenerateSpawnTransforms] 스폰할 적이 없음"));
		return Results;
	}

	Results.Reserve(TotalCount);

    // 1. 적 타입 배열 생성 (셔플 포함)
    TArray<TSubclassOf<ACEnemyCharacterBase>> EnemyTypeArray = GenerateEnemyTypeArray();

    if (EnemyTypeArray.Num() != TotalCount)
    {
        CLog::Log(FString::Printf(TEXT("[ACEnemySpawnZone::GenerateSpawnTransforms] WARNING: 타입 배열 불일치 : %d, 실제 -> %d"), TotalCount, EnemyTypeArray.Num()));
        return Results;
    }

    // 2. 위치 생성
    TArray<FVector> Locations;
    Locations.Reserve(TotalCount);

    FVector ZoneCenter = GetActorLocation();
    FRotator ZoneRotation = GetActorRotation();

    for (int32 i = 0; i < TotalCount; i++)
    {
        int32 Attempts = 0;
        bool bFoundValidLocation = false;

        while (Attempts < MaxRetries && !bFoundValidLocation)
        {
            // 반경 내 랜덤 위치
            FVector RandomLocation = GetRandomLocationInRadius();

            // 다른 위치와 거리 체크
            if (IsLocationValid(RandomLocation, Locations))
            {
                // 지면 추적
                if (bTraceForGround)
                {
                    if (TraceForGround(RandomLocation))
                    {
                        Locations.Add(RandomLocation);
                        bFoundValidLocation = true;
                    }
                }
                else
                {
                    RandomLocation.Z = ZoneCenter.Z + HeightOffset;
                    Locations.Add(RandomLocation);
                    bFoundValidLocation = true;
                }
            }

            Attempts++;
        }

        // 최대 재시도 초과
        if (!bFoundValidLocation)
        {
            CLog::Log(FString::Printf(TEXT("[ACEnemySpawnZone::GenerateSpawnTransforms] WARNING: %d번 시도 후 유요한 위치를 찾지 못함 (적 %d/%d)"), MaxRetries, i + 1, TotalCount));

            // 최소한이라도 스폰
            FVector FallbackLocation = GetRandomLocationInRadius();
            if (bTraceForGround)
            {
                TraceForGround(FallbackLocation);
            }
            else
            {
                FallbackLocation.Z = ZoneCenter.Z + HeightOffset;
            }
            Locations.Add(FallbackLocation);
        }
    }

    // 3. Transform 생성 (위치 + 타입)
    for (int32 i = 0; i < Locations.Num(); i++)
    {
        // 랜덤 Yaw 회전
        FRotator RandomRotation = ZoneRotation;
        RandomRotation.Yaw += FMath::FRandRange(-180.f, 180.f);

        FTransform SpawnTransform(RandomRotation, Locations[i], FVector::OneVector);
        TSubclassOf<ACEnemyCharacterBase> EnemyClass = EnemyTypeArray[i];

        Results.Add(FSpawnTransformInfo(SpawnTransform, EnemyClass));
    }

    CLog::Log(FString::Printf(TEXT("[ACEnemySpawnZone::GenerateSpawnTransforms]: %d개의 스폰 위치 생성 완료"), Results.Num()));
    return Results;
}

TArray<FSpawnTransformInfo> ACEnemySpawnZone::GenerateFixedSpawnTransforms(TSubclassOf<ACEnemyCharacterBase> EnemyClass,
	int32 Count)
{
	TArray<FSpawnTransformInfo> Results;
	if (!EnemyClass || Count <= 0)
	{
		return Results;
	}
	
	Results.Reserve(Count);
	TArray<FVector> Locations;
	Locations.Reserve(Count);
	
	FVector ZoneCenter = GetActorLocation();
	FRotator ZoneRotation = GetActorRotation();
	
	for (int32 i = 0; i < Count; ++i)
	{
		int32 Attempts = 0;
		bool bFoundValidLocation = false;
			
		while (Attempts < MaxRetries && !bFoundValidLocation)
		{
			FVector RandomLocation = GetRandomLocationInRadius();
					
			if (IsLocationValid(RandomLocation, Locations))
			{
				if (bTraceForGround)
				{
					if (TraceForGround(RandomLocation))
					{
						Locations.Add(RandomLocation);
						bFoundValidLocation = true;
					}
				}
				else
				{
					RandomLocation.Z = ZoneCenter.Z + HeightOffset;
					Locations.Add(RandomLocation);
					bFoundValidLocation = true;
				}
			}
					
			Attempts++;
		}
			
		if (!bFoundValidLocation)
		{
			FVector FallbackLocation = GetRandomLocationInRadius();
			if (bTraceForGround)
			{
				TraceForGround(FallbackLocation);
			}
			else
			{
				FallbackLocation.Z = ZoneCenter.Z + HeightOffset;
			}
			Locations.Add(FallbackLocation);
		}
	}
	
	for (const FVector& Location : Locations)
	{
		FRotator RandomRotation = ZoneRotation;
		RandomRotation.Yaw += FMath::FRandRange(-180.f, 180.f);
			
		FTransform SpawnTransform(RandomRotation, Location, FVector::OneVector);
		Results.Add(FSpawnTransformInfo(SpawnTransform, EnemyClass));
	}
	
	return Results;
}

TArray<TSubclassOf<ACEnemyCharacterBase>> ACEnemySpawnZone::GenerateEnemyTypeArray() const
{
	TArray<TSubclassOf<ACEnemyCharacterBase>> Result;

	int32 TotalCount = GetTotalEnemyCount();
	Result.Reserve(TotalCount);

	// EnemyTypes를 순회하며 Count만큼 추가
	for (const FEnemySpawnInfo& Info : EnemyTypes)
	{
		if (!Info.EnemyClass)
		{
			CLog::Log(TEXT("[ACEnemySpawnZone::GenerateEnemyTypeArray] EnemyTypes에 null 클래스 있음"));
			continue;
		}

		for (int32 i = 0; i < Info.Count; i++)
		{
			Result.Add(Info.EnemyClass);
		}
	}

	if (bShuffleEnemyTypes && Result.Num() > 1)
	{
		for (int32 i = Result.Num() - 1; i >= 0; i--)
		{
			int32 j = FMath::RandRange(0, i);
			Result.Swap(i, j);
		}
		
		CLog::Log(TEXT("[ACEnemySpawnZone::GenerateEnemyTypeArray] 적 타입 셔플 완료"));
	}

	return Result;
}

FVector ACEnemySpawnZone::GetRandomLocationInRadius() const
{
	FVector ZoneCenter = GetActorLocation();

	// 원형 균등 분포
	float RandomAngle = FMath::FRandRange(0.0f, 2.0f * PI);
	float RandomDistance = FMath::Sqrt(FMath::FRand()) * SpawnRadius;

	float X = ZoneCenter.X + RandomDistance * FMath::Cos(RandomAngle);
	float Y = ZoneCenter.Y + RandomDistance * FMath::Sin(RandomAngle);
	float Z = ZoneCenter.Z;

	return FVector(X, Y, Z);
}

bool ACEnemySpawnZone::IsLocationValid(const FVector& Location, const TArray<FVector>& ExistingLocations) const
{
	for (const FVector& ExistingLocation : ExistingLocations)
	{
		float Distance = FVector::Dist2D(Location, ExistingLocation);
		if (Distance < MinDistanceBetweenEnemies)
		{
			return false;
		}
	}

	return true;
}

bool ACEnemySpawnZone::TraceForGround(FVector& Location) const
{
	FVector Start = Location + FVector(0, 0, 500.0f);
	FVector End = Location - FVector(0.0f, 0.0f, 1000.0f);

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		Start,
		End,
		ECC_WorldStatic,
		QueryParams);

	if (bHit)
	{
		Location = HitResult.Location + FVector(0.0f, 0.0f, HeightOffset);
		return true;
	}

	return false;
}

void ACEnemySpawnZone::DrawDebugSpawnLocation()
{
#if WITH_EDITOR
	if (!GetWorld())
		return;

	FlushPersistentDebugLines(GetWorld());

	// 반경 표시
	DrawDebugSphere(
		GetWorld(),
		GetActorLocation(),
		SpawnRadius,
		32,
		FColor::Cyan,
		true,
		-1.f,
		0,
		2.f);
	
#endif
}
