// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CEnemySpawnZone.generated.h"

class USphereComponent;
class ACEnemyCharacterBase;

// 적 스폰 정보
USTRUCT(BlueprintType)
struct FEnemySpawnInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	TSubclassOf<ACEnemyCharacterBase> EnemyClass;

	// 스폰할 개수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "1"))
	int32 Count = 1;

	FEnemySpawnInfo() : EnemyClass(nullptr), Count(1) {}
	FEnemySpawnInfo(TSubclassOf<ACEnemyCharacterBase> InClass, int32 InCount) : EnemyClass(InClass), Count(InCount) {}
};

// 스폰 Transform + 적 타입
// 실제 스폰에 사용
USTRUCT()
struct FSpawnTransformInfo
{
	GENERATED_BODY()

	FTransform Transform;
	TSubclassOf<ACEnemyCharacterBase> EnemyClass;

	FSpawnTransformInfo() : Transform(FTransform::Identity), EnemyClass(nullptr) {}
	FSpawnTransformInfo(const FTransform& InTransform, TSubclassOf<ACEnemyCharacterBase> InClass) : Transform(InTransform), EnemyClass(InClass) {}
};

UCLASS()
class POSISONFROG_API ACEnemySpawnZone : public AActor
{
	GENERATED_BODY()

public:
	ACEnemySpawnZone();

protected:
	virtual void BeginPlay() override;

#if WITH_EDITOR
	virtual void OnConstruction(const FTransform& Transform) override;
#endif
	
public:
	// ──────────── 스폰 설정 ────────────
	UPROPERTY(EditAnywhere, Category = "Spawn|Setup")
	int32 SectionID = 1;

	UPROPERTY(EditAnywhere, Category = "Spawn|Setup")
	TArray<FEnemySpawnInfo> EnemyTypes;

	// 스폰 반경
	UPROPERTY(EditAnywhere, Category = "Spawn|Area")
	float SpawnRadius = 1000.0f;

	// 스폰 적 간 최소 거리 (Cm)
	UPROPERTY(EditAnywhere, Category = "Spawn|Area")
	float MinDistanceBetweenEnemies = 150.0f;

	// 지면 오프셋 (Cm)
	UPROPERTY(EditAnywhere, Category = "Spawn|Area")
	float HeightOffset = 100.0f;

	UPROPERTY(EditAnywhere, Category = "Spawn|Area")
	bool bTraceForGround = true;

	// 최대 재시도 횟수 - 겹치지 않는 위치 찾기 실패 시
	UPROPERTY(EditAnywhere, Category = "Spawn|Advanced")
	int32 MaxRetries = 50;

	// 적 타입 셔플
	// true면 타입 순서를 랜덤하게 배치
	UPROPERTY(EditAnywhere, Category = "Spawn|Advanced")
	bool bShuffleEnemyTypes = true;
	
	// ──────────── 시각화 ────────────
	// 스폰 범위 시각화
	UPROPERTY(EditAnywhere, Category = "Spawn|Visualization")
	USphereComponent* SpawnAreaSphere;

	// 에디터에서 스폰 위치 미리보기
	UPROPERTY(EditAnywhere, Category = "Spawn|Debug")
	bool bShowSpawnPreview = true;

public:
	// ──────────── 스폰 로직 ────────────
	int32 GetTotalEnemyCount() const;
	TArray<FSpawnTransformInfo> GenerateSpawnTransforms();

private:
	TArray<TSubclassOf<ACEnemyCharacterBase>> GenerateEnemyTypeArray() const;
	
	// 반경 내 랜덤 위치 생성
	FVector GetRandomLocationInRadius() const;

	// 다른 위치와 거리 체크
	bool IsLocationValid(const FVector& Location, const TArray<FVector>& ExistingLocations) const;

	// 지면 추적
	bool TraceForGround(FVector& Location) const;

	// 디버그 드로우
	void DrawDebugSpawnLocation();
};
