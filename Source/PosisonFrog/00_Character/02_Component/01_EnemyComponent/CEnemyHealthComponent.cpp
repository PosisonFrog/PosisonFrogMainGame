// Fill out your copyright notice in the Description page of Project Settings.


#include "CEnemyHealthComponent.h"

#include "99_Util/CLog.h"

void UCEnemyHealthComponent::OnDeathInternal()
{
	CLog::Log(FString::Printf(TEXT("UCEnemyHealthComponent::OnDeathInternal -> ['%s'] 사망"), *GetName()));
	OnDeath.Broadcast();
}
