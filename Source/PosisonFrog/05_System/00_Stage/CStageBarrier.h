// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CStageBarrier.generated.h"

class UBoxComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBarrierOpened, int32, SectionID);

/*
 * 스테이지 구간 사이 투명벽
 */
UCLASS()
class POSISONFROG_API ACStageBarrier : public AActor
{
	GENERATED_BODY()

public:
	ACStageBarrier();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

protected:
	bool bIsOpen = false;
	

	// 플레이어와의 거리에 따른 투명도 업데이트
	void UpdateOpacityByDistance();
	
	// 열릴 때 애니메이션/VFX 재생
	UFUNCTION()
	void PlayOpenEffects();

	// 닫힐 때 애니메이션/VFX 재생
	UFUNCTION()
	void PlayCloseEffects();

	// 벽이 열린 후 완전히 비활성화
	void FullyDeactivate();

public:

	// ──────────── 벽 제어 ────────────
	UFUNCTION()
	void OpenBarrier();

	UFUNCTION()
	void CloseBarrier();

	UFUNCTION()
	bool IsOpen() const { return bIsOpen; }
	
public:
	// ──────────── 구간 설정 ────────────
	UPROPERTY(EditAnywhere, Category = "Barrier")
	int32 SectionID = 1;

	// ──────────── 컴포넌트 ────────────
	UPROPERTY(EditAnywhere, Category = "Barrier")
	TObjectPtr<UBoxComponent> CollisionBox;

	UPROPERTY(EditAnywhere, Category = "Barrier")
	TObjectPtr<UStaticMeshComponent> BarrierMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;

protected:
	
	// ──────────── 거리 설정 ────────────
	// 1500이상이면 투명
	UPROPERTY(EditAnywhere, Category = "Barrier|Visibility", meta = (ClampMin = "0.0"))
	float MaxVisibilityDistance = 1500.0f;

	// 300 이하면 오퍼시티 최대 값으로 고정. 
	UPROPERTY(EditAnywhere, Category = "Barrier|Visibility", meta = (ClampMin = "0.0"))
	float MinVisibilityDistance = 300.0f;

	// 최대 투명도 (0=완전투명, 1=완전불투명)
	UPROPERTY(EditAnywhere, Category = "Barrier|Visibility", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxOpacity = 0.7f;

	// ──────────── 이벤트 ────────────
	UPROPERTY(EditAnywhere, Category = "Barrier|Events")
	FOnBarrierOpened OnBarrierOpened;
};