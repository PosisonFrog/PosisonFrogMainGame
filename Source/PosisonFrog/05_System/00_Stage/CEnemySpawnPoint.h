// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CEnemySpawnPoint.generated.h"

class ACEnemyCharacterBase;

UCLASS()
class POSISONFROG_API ACEnemySpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	ACEnemySpawnPoint();

protected:
	virtual void BeginPlay() override;

public:
	// 랜덤 오프셋이 적용된 최종 스폰 위치 계산
	UFUNCTION()
	FVector GetRandomSpawnLocation() const;

	// 스폰 회전값 가져오기
	UFUNCTION()
	FRotator GetSpawnRotation() const;

	// 최종 스폰 위치 값
	UFUNCTION()
	FTransform GetSpawnTransform() const;

public:
	// 어떤 적을 스폰할지
	UPROPERTY(EditAnywhere, Category = "Spawn")
	TSubclassOf<ACEnemyCharacterBase> EnemyClass;

	// 어느 구간에 속하는지
	UPROPERTY(VisibleAnywhere, Category = "Spawn")
	int32 SectionID = 1;

	// 랜덤 오프셋 반경 - RandomOffsetRadius 반경 안에서 랜덤 위치에 스폰
	UPROPERTY(EditAnywhere, Category = "Spawn", meta=(ClampMin = "0"))
	float RandomOffsetRadius = 100.0f;

	// 스폰 포인트의 회전값 사용 여부
	UPROPERTY(EditAnywhere, Category = "Spawn")
	bool bUseSpawnPointRotation = true;

	// 스폰 딜레이
	UPROPERTY(EditAnywhere, Category = "Spawn|Advanced", meta = (ClampMin = "0"))
	float SpawnDelay = 0.0f;
};
