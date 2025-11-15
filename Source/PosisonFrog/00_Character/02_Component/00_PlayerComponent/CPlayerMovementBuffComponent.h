#pragma once

#include "CoreMinimal.h"
#include "00_Character/02_Component/CBaseMovementBuffComponent.h"
#include "Components/ActorComponent.h"
#include "CPlayerMovementBuffComponent.generated.h"

class UCharacterMovementComponent;

USTRUCT()
struct FActiveSpeedBuff
{
	GENERATED_BODY()

	UPROPERTY() float Multiplier = 1.f;   // 1.15 = +15%
	UPROPERTY() float ExpireTime = 0.f;   // World Time 기준
	FTimerHandle TimerHandle;
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class POSISONFROG_API UCPlayerMovementBuffComponent : public UCBaseMovementBuffComponent
{
	GENERATED_BODY()

public:
	UCPlayerMovementBuffComponent();

	// 이속 버프 추가 (멀티스택 허용, 정책: 최댓값 우선)
	UFUNCTION(BlueprintCallable, Category = "Buff|Speed")
	void AddSpeedBuff(float Multiplier, float DurationSeconds);

	// 기준 속도 갱신(플레이어의 MaxWalkSpeed 기본값이 바뀌면 호출)
	UFUNCTION(BlueprintCallable, Category = "Buff|Speed")
	void SetBaseMaxWalkSpeed(float InBaseSpeed);

	UFUNCTION(BlueprintPure, Category = "Buff|Speed")
	float GetCurrentSpeedMultiplier() const { return CurrentMaxMultiplier; }
	/**
		+     * 외부에서 추가 곱연산을 적용할 때 사용(공격 슬로우 등)
		+     * - 1.0 = 기본, < 1.0 = 감속, > 1.0 = 추가 가속
		+     */

	void SetAttackSlowMultiplier(float Multiplier);
	float GetAttackSlowMultiplier() const { return AttackSlowMultiplier; }
	
	/**
     * 전투 이외 가속(예: 비전투 이동 속도 보너스) 적용
     */
	void SetIdleSpeedMultiplier(float Multiplier);
	float GetIdleSpeedMultiplier() const { return IdleSpeedMultiplier; }
	
	/**
	    * (Deprecated) 이전 추가 배율 API 유지 – 공격 슬로우용으로 위임
    */
	    UE_DEPRECATED(5.4, "Use SetAttackSlowMultiplier instead")
	    void SetAdditionalMultiplier(float Multiplier) { SetAttackSlowMultiplier(Multiplier); }
	
	    float GetAdditionalMultiplier() const { return AttackSlowMultiplier * IdleSpeedMultiplier; }
	
protected:
	virtual void BeginPlay() override;

private:
	void OnBuffExpired(int32 Index);
	void RecomputeAndApply();
	void ApplyEffectiveMultiplier();

private:
	UPROPERTY() TArray<FActiveSpeedBuff> ActiveBuffs;

	UPROPERTY() UCharacterMovementComponent* MoveComp = nullptr;
	UPROPERTY() float BaseMaxWalkSpeed = 0.f;
	UPROPERTY() float CurrentMaxMultiplier = 1.f;
	UPROPERTY() float AttackSlowMultiplier = 1.f;
	UPROPERTY() float IdleSpeedMultiplier = 1.f;
};


