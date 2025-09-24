#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CPlayerWidget.generated.h"

class UTextBlock;
class UCPlayerHpBarWidget;
class UCSkillIconUIWidget;
class UCTimeCooldownSkillIconWidget;
class UCUltimateSkillIconWidget;

/**
 * 플레이어 상단 HUD(HP/대시 쿨타임/궁극기)를 갱신하는 위젯
 */
UCLASS()
class POSISONFROG_API UCPlayerWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** HP UI 갱신 */
    UFUNCTION(BlueprintCallable)
    void UpdateHpBar(float Current, float Max);

    /** 대시 쿨타임 갱신 (RemainingSeconds가 0이하이면 자동으로 READY 처리) */
    UFUNCTION(BlueprintCallable)
    void UpdateDashCooldown(float RemainingSeconds, float TotalSeconds);

    /** 대시 READY 상태로 전환 (아이콘/텍스트 모두 정리) */
    UFUNCTION(BlueprintCallable)
    void SetDashReady();

    /** 궁극기(게이지) 갱신: 0.0~Max 사이 값 반영 */
    UFUNCTION(BlueprintCallable)
    void SetUltimatePoints(float UltimateCurrentPoints, float UltimateMaxPoints);

    /** 대시 쿨타임 텍스트를 보일지 여부(READY 포함) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dash")
    bool bShowDashText = true;

    /** READY 표시 문자열(다국어/프로젝트 스타일에 맞게 교체 가능) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dash")
    FText ReadyText = FText::FromString(TEXT("READY"));

protected:
    virtual void NativeConstruct() override;

    // ───────── 바운드 위젯(있으면 자동 바인딩) ─────────

    /** 남은 쿨타임 숫자/READY 표시용 텍스트 */
    UPROPERTY(meta=(BindWidgetOptional))
    TObjectPtr<UTextBlock> DashCooldownText = nullptr;

    /** (구) 일반 스킬 아이콘 위젯 */
    //UPROPERTY(meta=(BindWidgetOptional))
    //TObjectPtr<UCSkillIconUIWidget> WBP_DashSkillIconUIWidget = nullptr;

    /** (신) 원형 타이머 기반 스킬 아이콘 위젯 */
    UPROPERTY(meta=(BindWidgetOptional))
    TObjectPtr<UCTimeCooldownSkillIconWidget> WBP_DashSkillIconWidget = nullptr;

    /** 궁극기(게이지) 아이콘 위젯 */
    UPROPERTY(meta=(BindWidgetOptional))
    TObjectPtr<UCUltimateSkillIconWidget> WBP_UltimateSkillIconWidget = nullptr;

    /** HP 바 루트 위젯 */
    UPROPERTY(meta=(BindWidgetOptional))
    TObjectPtr<UCPlayerHpBarWidget> WBP_PlayerHpBar = nullptr;

private:
    /** 내부 헬퍼: 대시 쿨다운 프로그레스(아이콘들)를 일관되게 업데이트 */
    void UpdateDashProgress_Internal(float ElapsedRatio, float TotalSeconds);

    /** 내부 헬퍼: 대시 텍스트를 “X.Xs”로 포맷해서 표시 */
    void ShowDashTextSeconds_Internal(float RemainingSeconds);

    /** 내부 헬퍼: 대시 텍스트를 READY 표기로 표시 */
    void ShowDashTextReady_Internal();
};

