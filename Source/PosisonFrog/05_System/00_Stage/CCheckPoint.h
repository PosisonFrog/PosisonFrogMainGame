// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CCheckPoint.generated.h"

UCLASS()
class POSISONFROG_API ACCheckPoint : public AActor
{
	GENERATED_BODY()

public:
	ACCheckPoint();

protected:
	virtual void BeginPlay() override;
	
public:
	// ──────────── 체크포인트 제어 ────────────
	// 체크포인트 활성화
	UFUNCTION()
	void ActivateCheckPoint();

	// 체크포인트 비활성화
	UFUNCTION()
	void DeactivateCheckPoint();

	// 활성화 상태 확인
	UFUNCTION()
	bool IsActivated() const { return bIsActivated; }

	// 리스폰 위치 가져오기
	UFUNCTION()
	FVector GetRespawnLocation() const;

	// 리스폰 방향 가져오기
	UFUNCTION()
	FRotator GetRespawnRotation() const;

	// 리스폰 위치 가져오기
	UFUNCTION()
	FTransform GetRespawnTransform() const;

protected:
	bool bIsActivated = false;
	
public:
	UPROPERTY(EditAnywhere, Category = "CheckPoint")
	int32 SectionID = 1;

	UPROPERTY(EditAnywhere, Category = "CheckPoint")
	FVector RespawnLocation;

	UPROPERTY(EditAnywhere, Category = "CheckPoint")
	FRotator RespawnRotation;
};
