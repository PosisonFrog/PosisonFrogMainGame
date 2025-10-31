// Fill out your copyright notice in the Description page of Project Settings.


#include "CCheckPoint.h"


ACCheckPoint::ACCheckPoint()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ACCheckPoint::BeginPlay()
{
	Super::BeginPlay();

	// 리스폰 Location이 설정 안되어 있으면 현재 위치 사용
	if (RespawnLocation.IsNearlyZero())
	{
		RespawnLocation = GetActorLocation();
	}

	// 리스폰 Rotation이 설정 안되어 있으면 현재 위치 사용
	if (RespawnRotation.IsNearlyZero())
	{
		RespawnRotation = GetActorRotation();
	}
}

void ACCheckPoint::ActivateCheckPoint()
{
	if (bIsActivated)
		return;

	bIsActivated = true;
}

void ACCheckPoint::DeactivateCheckPoint()
{
	if (!bIsActivated)
		return;

	bIsActivated = false;
}

FVector ACCheckPoint::GetRespawnLocation() const
{
	if (!RespawnLocation.IsNearlyZero())
		return RespawnLocation;

	return GetActorLocation();
}

FRotator ACCheckPoint::GetRespawnRotation() const
{
	if (!RespawnRotation.IsNearlyZero())
		return RespawnRotation;

	return GetActorRotation();
}

FTransform ACCheckPoint::GetRespawnTransform() const
{
	return FTransform(GetRespawnRotation(), GetRespawnLocation(), FVector::OneVector);
}
