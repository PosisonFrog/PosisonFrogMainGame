// CHealthComponent.h

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CHealthComponent.generated.h"

class UCUltimateBuffComponent;
class UCPlayerStatAssetData;

// 체력 변경/사망 이벤트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChanged, float, Current, float, Max);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeath);

/**
 * 단순/견고한 체력 컴포넌트
 * - BeginPlay에서 에셋(있다면)으로 MaxHealth 적용
 * - bStartAtMaxHealth에 따라 현재 HP 초기화
 * - Healing/Damage는 음수/0 무시, 클램프 보장, 변화 없으면 이벤트 미브로드캐스트
 * - 0 도달 시 IsDead=true 전환 + OnDeath 1회 브로드캐스트
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class POSISONFROG_API UCHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCHealthComponent();

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
	float Damage(float InAmount);

	/**
	 * 최대체력 변경
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
	/** 내부 통합 경로: 클램프 + 상태전이 + 이벤트 브로드캐스트 */
	void SetHealthClamped(float NewValue);

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

private:
	// 캐싱용
	UPROPERTY() ACharacter* OwnerChar = nullptr;
};

