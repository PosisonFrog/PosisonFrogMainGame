// Fill out your copyright notice in the Description page of Project Settings.


#include "CEnemyWeaponBase.h"

#include "99_Util/CLog.h"


ACEnemyWeaponBase::ACEnemyWeaponBase()
{
	
}

void ACEnemyWeaponBase::BeginPlay()
{
	Super::BeginPlay();
}

bool ACEnemyWeaponBase::ShouldHitActor(AActor* OtherActor) const
{
	if (!Super::ShouldHitActor(OtherActor))
		return false;

	if (!PlayerTag.IsNone() && !OtherActor->ActorHasTag(PlayerTag))
	{
		if (bDebugLog)
			CLog::Log(FString::Printf(TEXT("ACEnemyWeaponBase : 플레이어 태그 없음 - %s"), *GetNameSafe(OtherActor)));

		return false;
	}

	return true;
}
