// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CPlayerStatAssetData.generated.h"

/**
 * 
 */
UCLASS()
class POSISONFROG_API UCPlayerStatAssetData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat")
	float MaxHp = 100.0f;
};
