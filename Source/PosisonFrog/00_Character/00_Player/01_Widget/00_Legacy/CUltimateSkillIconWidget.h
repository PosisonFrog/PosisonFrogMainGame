#pragma once

#include "CoreMinimal.h"
#include "00_Character/00_Player/01_Widget/00_Legacy/CSkillIconBaseWidget.h"
#include "CUltimateSkillIconWidget.generated.h"

class UProgressBar;
class UWidgetAnimation;

/**
 * 궁극기 아이콘 위젯
 * - 게이지 비율 SetRatio / SetCooldown 지원
 * - 비율에 따라 색상 자동 보간(Empty→Mid→Full)
 * - 가득 찼을 때 펄스 애니메이션(선택)
 * - 위젯 생성 전 업데이트도 안전(값 캐시 후 Construct 시 반영)
 */
UCLASS()
class POSISONFROG_API UCUltimateSkillIconWidget : public UCSkillIconBaseWidget
{
	GENERATED_BODY()

public:
	/** 0~1 비율로 직접 설정 */
	UFUNCTION(BlueprintCallable, Category="Ultimate")
	void SetRatio(float InRatio);

	/** (현재/최대)로 비율 설정 */
	UFUNCTION(BlueprintCallable, Category="Ultimate")
	void SetCooldown(float Current, float Max)
	{
		const float R = (Max > 0.f) ? (Current / Max) : 0.f;
		SetRatio(R);
	}

	/** 사용 가능/불가 시각 효과 (회색 처리 등) */
	UFUNCTION(BlueprintCallable, Category="Ultimate")
	void SetUsable(bool bInUsable);

	/** 현재 비율 조회 */
	UFUNCTION(BlueprintPure, Category="Ultimate")
	float GetRatio() const { return Ratio; }

protected:
	virtual void NativeConstruct() override;
	virtual void SynchronizeProperties() override;

	/** 꽉 찼을 때 살짝 반짝이는 연출 (UMG 애니 넣으면 자동 사용) */
	UPROPERTY(Transient, meta=(BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Pulse = nullptr;

	/** 비율에 따라 색 자동 보간할지 */
	UPROPERTY(EditAnywhere, Category="Ultimate|Visual")
	bool bAutoColorByRatio = true;

	/** 가득 찼을 때 펄스 재생할지 */
	UPROPERTY(EditAnywhere, Category="Ultimate|Visual")
	bool bPulseWhenFull = true;

	/** Full로 취급하는 임계치 (>= 임계치면 풀충전) */
	UPROPERTY(EditAnywhere, Category="Ultimate|Visual", meta=(ClampMin="0.0", ClampMax="1.0"))
	float FullThreshold = 0.999f;

	/** 비율 0%일 때 색 */
	UPROPERTY(EditAnywhere, Category="Ultimate|Visual")
	FLinearColor Color_Empty = FLinearColor(0.15f, 0.15f, 0.15f, 1.f);

	/** 비율 50% 근처 색 */
	UPROPERTY(EditAnywhere, Category="Ultimate|Visual")
	FLinearColor Color_Mid = FLinearColor(0.10f, 0.55f, 1.00f, 1.f);

	/** 비율 100% 색 */
	UPROPERTY(EditAnywhere, Category="Ultimate|Visual")
	FLinearColor Color_Full = FLinearColor(0.95f, 0.85f, 0.10f, 1.f);

	/** 사용 불가 시 틴트(회색) 계열 */
	UPROPERTY(EditAnywhere, Category="Ultimate|Visual")
	FLinearColor Tint_Unusable = FLinearColor(0.45f, 0.45f, 0.45f, 0.75f);

private:
	void ApplyVisuals();            // 내부 시각 갱신
	void TryResolveWidgetRefs();    // SkillBar 없을 때 런타임 탐색

private:
	float Ratio = 0.f;              // 0~1 캐시
	bool  bUsable = true;           // 사용 가능 여부
	bool  bConstructed = false;     // NativeConstruct 완료 여부
};

