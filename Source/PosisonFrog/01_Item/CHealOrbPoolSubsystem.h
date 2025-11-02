// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CHealOrbPoolSubsystem.generated.h"

class ACHealOrb;

// 기존 : HUD/BP
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnOrbCountersChanged, int32, ActiveOrbs, int32, TotalPicked);

UCLASS()
class POSISONFROG_API UCHealOrbPoolSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// HUD에서 구독
	UPROPERTY(BlueprintAssignable, Category="HealOrb|Pool")
	FOnOrbCountersChanged OnCountersChanged;

	// 풀 기본 클래스 지정(프로젝트 초기화 시 1회)
	UFUNCTION(BlueprintCallable, Category="HealOrb|Pool")
	void SetOrbClass(TSubclassOf<ACHealOrb> InClass) { OrbClass = InClass; }

	// 미리 생성(옵션)
	UFUNCTION(BlueprintCallable, Category="HealOrb|Pool")
	void Prewarm(UWorld* World, int32 Count);

	// 오브 획득(스폰/풀에서 꺼냄)
	UFUNCTION(BlueprintCallable, Category="HealOrb|Pool")
	ACHealOrb* Acquire(UWorld* World, const FTransform& Xform, AActor* PreferredTarget = nullptr);

	// 오브 반환(ReleaseOrb(true) 내부 또는 외부 호출)
	UFUNCTION(BlueprintCallable, Category="HealOrb|Pool")
	void Release(ACHealOrb* Orb);

	UFUNCTION()
	void OnPreLoadMap(const FString& MapName);

	UFUNCTION()
	void OnWorldTearDown(UWorld* World);
	
	// 레벨 전환 시 풀 정리
	UFUNCTION(Blueprintable, Category = "HealOrb|Pool")
	void ClearPool();
	
	// 픽업 통지(오브에서 콜백)
	void NotifyPicked(ACHealOrb* Orb);

	// 조회
	UFUNCTION(BlueprintPure, Category="HealOrb|Pool")
	int32 GetActiveCount() const { return ActivePool.Num(); }

	UFUNCTION(BlueprintPure, Category="HealOrb|Pool")
	int32 GetTotalPicked() const { return TotalPicked; }

protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	void BroadcastCounters();

private:
	UPROPERTY()
	TSubclassOf<ACHealOrb> OrbClass;

	UPROPERTY()
	TArray<TObjectPtr<ACHealOrb>> InactivePool;

	UPROPERTY()
	TArray<TObjectPtr<ACHealOrb>> ActivePool;

	int32 TotalPicked = 0;

	bool bIsShuttingDown = false;
};
