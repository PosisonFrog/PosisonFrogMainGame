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
	// Sets default values for this actor's properties
	ACStageBarrier();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

protected:
	bool bIsOpen = false;

	// ──────────── 이벤트 ────────────
	UPROPERTY(EditAnywhere, Category = "Barrier|Events")
	FOnBarrierOpened OnBarrierOpened;

protected:
	// 열릴 때 애니메이션/VFX 재생
	UFUNCTION()
	void PlayOpenEffects();

	// 닫힐 때 애니메이션/VFX 재생
	UFUNCTION()
	void PlayCloseEffects();

	// 벽이 열린 후 완전히 비활성화
	void FullyDeactivate();
	
public:
	// ──────────── 구간 설정 ────────────
	UPROPERTY(EditAnywhere, Category = "Barrier")
	int32 SectionID = 1;

	// ──────────── 컴포넌트 ────────────
	UPROPERTY(EditAnywhere, Category = "Barrier")
	TObjectPtr<UBoxComponent> CollisionBox;

	UPROPERTY(EditAnywhere, Category = "Barrier")
	TObjectPtr<UStaticMeshComponent> BarrierMesh;

	// ──────────── 벽 제어 ────────────
	UFUNCTION()
	void OpenBarrier();

	UFUNCTION()
	void CloseBarrier();

	UFUNCTION()
	bool IsOpen() const { return bIsOpen; }
};
