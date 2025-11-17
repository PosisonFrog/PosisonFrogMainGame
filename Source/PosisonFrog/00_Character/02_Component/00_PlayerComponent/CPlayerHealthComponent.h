// CHealthComponent.h

#pragma once

#include "CoreMinimal.h"
#include "00_Character/02_Component/CBaseHealthComponent.h"
#include "CPlayerHealthComponent.generated.h"

class UCPlayerStatAssetData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnOverHealChanged, float, CurrentOverHeal, float, MaxOverHeal);
/**
 * 플레이어 체력 컴포넌트
 * - 버프 시스템 연동 (IBuffable 인터페이스)
 * - UI 갱신은 ACPlayerCharacter에서 처리
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class POSISONFROG_API UCPlayerHealthComponent : public UCBaseHealthComponent
{
	GENERATED_BODY()

public:
	virtual float Healing(float InAmount) override;

	// 버프 시스템 통합
	virtual float Damage(float InAmount) override;

	// 플레이어 전용 사망 처리
	virtual void OnDeathInternal() override;

	// 오버힐 조회
	float GetTotalHealth() const {return CurrentHealth + CurrentOverHeal;}

	UPROPERTY(BlueprintAssignable, Category = "Health|Events")
	FOnOverHealChanged OnOverHealChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// 오버힐 감소 시스템
	void StartOverHealDecayTimer();
	void DecayOverHeal();
	
protected:
	// ──────────── 오버힐 변수 ────────────
	UPROPERTY(EditAnywhere, Category = "Health|OverHeal", meta = (ClampMin = "0.0"))
	float OverHealMax = 35.0f;
	
	UPROPERTY(EditAnywhere, Category = "Health|OverHeal")
	bool bOverHealDecayEnabled = true;

	UPROPERTY(EditAnywhere, Category = "Health|OverHeal", meta = (EditCondition = "bOverHealDecayEnabled", ClampMin = "0.1"))
	float OverHealDecayDelay = 3.0f;
	
	UPROPERTY(EditAnywhere, Category = "Health|OverHeal", meta = (EditCondition = "bOverHealDecayEnabled", ClampMin = "0.1"))
	float OverHealDecayRate = 5.0f; // 초당 감소량
	
	// ──────────── 오버힐 상태 ────────────
	UPROPERTY(VisibleAnywhere, Category = "Health|OverHeal")
	float CurrentOverHeal = 0.0f;

private:
	FTimerHandle OverHealDecayTimerHandle;
};

