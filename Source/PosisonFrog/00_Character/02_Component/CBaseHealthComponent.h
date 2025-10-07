// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CBaseHealthComponent.generated.h"

class UCPlayerStatAssetData;
// 체력 변경/사망 이벤트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChanged, float, Current, float, Max);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeath);

/*
 * 체력 컴포넌트 베이스
 * - 공통 체력 관리 로직
 * - 데미지 배율은 자식 컴포넌트에서 오버라이드
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class POSISONFROG_API UCBaseHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UCBaseHealthComponent();

	// 생명주기
	virtual void BeginPlay() override;

	// --- 조회 ---
	UFUNCTION(BlueprintCallable, Category = "Health")
	float GetHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintCallable, Category = "Health")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintCallable, Category = "Health")
	bool IsDead() const { return bIsDead; }

	// --- 변경 ---
	/** 회복: 실제 회복된 양을 반환 */
	UFUNCTION(BlueprintCallable, Category = "Health")
	float Healing(float InAmount);

	/** 피해: 실제 피해량을 반환 */
	UFUNCTION(BlueprintCallable, Category = "Health")
	virtual float Damage(float InAmount);

	/**
	 * @param NewMax	      최대체력 변경
	 * @param bClampCurrent   현재 HP를 0..NewMax로 클램프
	 * @param bResetToMax     현재 HP를 NewMax로 즉시 세팅
	 */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void SetMaxHealth(float NewMax, bool bClampCurrent = true, bool bResetToMax = false);

	// --- 이벤트 ---
	UPROPERTY(BlueprintAssignable, Category = "Health|Events")
	FOnHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Health|Events")
	FOnDeath OnDeath;
	
protected:
	// --- 가상 함수 ---
	// 체력 초기화 로직
	virtual void InitializeHealth();

	// 사망 처리
	virtual void OnDeathInternal() {}

	// --- 내부 헬퍼 ---
	void SetHealthClamped(float NewValue);

protected:
	// --- 설정값 ---
	UPROPERTY(EditDefaultsOnly, Category = "Health", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.f;

	/** 에디터에서 값 지정 시 bStartAtMaxHealth=false로 두면 그대로 시작 */
	UPROPERTY(EditAnywhere, Category = "Health", meta = (ClampMin = "0.0"))
	float CurrentHealth = 50.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Health")
	bool bIsDead = false;

	/** BeginPlay에서 MaxHealth/CurrentHealth 초기화 전략 */
	UPROPERTY(EditAnywhere, Category = "Health")
	bool bStartAtMaxHealth = true;

	/** 플레이어/적 등 초기 스탯이 담긴 에셋(있으면 MaxHealth를 여기서 가져옴) */
	UPROPERTY(EditDefaultsOnly, Category = "Health")
	TObjectPtr<const UCPlayerStatAssetData> PlayerStatAssetData = nullptr;

	// --- 캐싱 ---
	UPROPERTY() TWeakObjectPtr<ACharacter> OwnerChar = nullptr;
};
