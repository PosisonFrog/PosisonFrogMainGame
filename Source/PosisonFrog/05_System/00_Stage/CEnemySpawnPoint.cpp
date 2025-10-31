// Fill out your copyright notice in the Description page of Project Settings.


#include "CEnemySpawnPoint.h"

#include "00_Character/01_Enemy/CEnemyCharacterBase.h"
#include "99_Util/CLog.h"


// Sets default values
ACEnemySpawnPoint::ACEnemySpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;
}

// Called when the game starts or when spawned
void ACEnemySpawnPoint::BeginPlay()
{
	Super::BeginPlay();

	if (!EnemyClass)
		CLog::Log(FString::Printf(TEXT("[SpawnPoint] %s EnemyClass 세팅이 안됨"), *GetName()));
}

FVector ACEnemySpawnPoint::GetRandomSpawnLocation() const
{
	FVector BaseLocation = GetActorLocation();

	if (RandomOffsetRadius > 0.0f)
	{
		FVector2D RandomCircle = FMath::RandPointInCircle(RandomOffsetRadius);

		BaseLocation.X += RandomCircle.X;
		BaseLocation.Y += RandomCircle.Y;
	}

	return BaseLocation;
}

FRotator ACEnemySpawnPoint::GetSpawnRotation() const
{
	if (bUseSpawnPointRotation)
	{
		return GetActorRotation();
	}
	else
	{
		return FRotator(0.0f, FMath::RandRange(0.0f, 360.0f), 0.0f);
	}
}

FTransform ACEnemySpawnPoint::GetSpawnTransform() const
{
	return FTransform(GetSpawnRotation(), GetRandomSpawnLocation(), FVector::OneVector);
}
