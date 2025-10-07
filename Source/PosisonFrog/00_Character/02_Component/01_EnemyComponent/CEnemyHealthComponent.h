// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "00_Character/02_Component/CBaseHealthComponent.h"
#include "CEnemyHealthComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class POSISONFROG_API UCEnemyHealthComponent : public UCBaseHealthComponent
{
	GENERATED_BODY()

protected:
	// 적 전용 사망 처리
	virtual void OnDeathInternal() override;
};
