// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CBossStageBarrier.generated.h"

class ACEnemyBossCharacter;
class ACStageManager;
class UBoxComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBossBarrierOpened);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBossBarrierClosed);

UCLASS()
class POSISONFROG_API ACBossStageBarrier : public AActor
{
	GENERATED_BODY()

public:
	ACBossStageBarrier();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;

protected:

	// 플레이어와의 거리에 따른 투명도 조절
	void UpdateOpacityByDistance();

public:
	// ──────────── 외부 명령 인터페이스 ────────────
	// 장벽 열기
	UFUNCTION()
	void OpenBarrier();

	// 장벽 닫기 (보스 전투 시작)
	UFUNCTION()
	void OnBossBattleStart();
	
	// 장벽 열기 (보스 사망)
	UFUNCTION()
	void OnBossBattleEnd();
	
	// 장벽이 열려있는지 확인
	UFUNCTION(BlueprintPure, Category = "BossBarrier")
	bool IsOpen() const { return bIsOpen; }

	// 보스 전투 활성화 여부 확인
	UFUNCTION(BlueprintPure, Category = "BossBarrier")
	bool IsBossBattleActive() const { return bIsBossBattleActive; }

protected:
	// 장벽 닫기
	void CloseBarrier();

	// 열림 효과 재생
	void PlayOpenEffects();

	// 닫힘 효과 재생
	void PlayCloseEffects();

	// 완전히 비활성화
	void FullyDeactivate();

public:
	UPROPERTY(EditAnywhere, Category = "Barrier")
	TObjectPtr<UBoxComponent> CollisionBox;
	
	UPROPERTY(EditAnywhere, Category = "Barrier")
	TObjectPtr<UStaticMeshComponent> BarrierMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;

	// 장벽이 완전히 비활성화되는 시간
	UPROPERTY(EditAnywhere, Category = "BossBarrier|Settings")
	float DeactivateDelay = 2.0f;

	// ──────────── 설정 ────────────
	// 이 배리어가 열리는 스테이지 ID
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BossBarrier")
	int32 TriggerStageID = 3;
	
protected:
	
	// ──────────── 이벤트 ────────────
	UPROPERTY(BlueprintAssignable, Category = "BossBarrier|Events")
	FOnBossBarrierOpened OnBarrierOpened;

	UPROPERTY(BlueprintAssignable, Category = "BossBarrier|Events")
	FOnBossBarrierClosed OnBarrierClosed;
	
	// ──────────── 거리 설정 ────────────
	UPROPERTY(EditAnywhere, Category = "BossBarrier|Visibility", meta = (ClampMin = "0.0"))
	float MaxVisibilityDistance = 1500.0f;

	UPROPERTY(EditAnywhere, Category = "BossBarrier|Visibility", meta = (ClampMin = "0.0"))
	float MinVisibilityDistance = 300.0f;

	UPROPERTY(EditAnywhere, Category = "BossBarrier|Visibility", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxOpacity = 0.7f;
	
private:
	// ──────────── 상태 ────────────
	bool bIsOpen = false;
	bool bIsBossBattleActive = false;

	FTimerHandle DeactivateTimer;
};