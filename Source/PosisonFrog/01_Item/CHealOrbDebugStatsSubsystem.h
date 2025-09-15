// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CHealOrbDebugStatsSubsystem.generated.h"

UENUM()
enum class EHealOrbEvent : uint8
{
	Spawn,
	DetectBegin,
	DetectEnd,
	Repath,
	Heal,
	Expire,
	PoolAcquire,
	PoolRelease
};

USTRUCT(BlueprintType)
struct FHealOrbEventCounts
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 Spawn = 0;
	UPROPERTY(BlueprintReadOnly) int32 DetectBegin = 0;
	UPROPERTY(BlueprintReadOnly) int32 DetectEnd = 0;
	UPROPERTY(BlueprintReadOnly) int32 Repath = 0;
	UPROPERTY(BlueprintReadOnly) int32 Heal = 0;
	UPROPERTY(BlueprintReadOnly) int32 Expire = 0;
	UPROPERTY(BlueprintReadOnly) int32 PoolAcquire = 0;
	UPROPERTY(BlueprintReadOnly) int32 PoolRelease = 0;

	void Reset() { *this = {}; }
};

UCLASS()
class POSISONFROG_API UCHealOrbDebugStatsSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void Increment(EHealOrbEvent Ev);
	FHealOrbEventCounts GetCounts() const { return Counts; }
	void ResetCounts() { Counts.Reset(); }

private:
	FHealOrbEventCounts Counts;
};