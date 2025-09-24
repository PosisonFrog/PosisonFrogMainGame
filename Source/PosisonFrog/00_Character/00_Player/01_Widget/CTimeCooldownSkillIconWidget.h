// CTimeCooldownSkillIconWidget.h

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
// 베이스가 따로 있다면 이걸로 교체하세요.
// #include "00_Character/00_Player/01_Widget/CSkillIconBaseWidget.h"
// class UCTimeCooldownSkillIconWidget : public UCSkillIconBaseWidget
// 로 바꿔도 됩니다.

#include "CTimeCooldownSkillIconWidget.generated.h"

class UImage;
class UProgressBar;
class UWidgetAnimation;
class UNiagaraSystem;
class USoundBase;

/**
 * 스킬(예: 대시) 쿨타임을 시간 기반으로 표시하는 아이콘 위젯
 * - 진행바(ProgressBar) 0~1로 ‘남은시간/최대시간’을 표현
 * - 쿨타임 시작 시 아이콘 그레이아웃, 종료 시 원복 및 SFX/VFX 출력
 * - 게임 코드(컴포넌트)에서 Start/Update/Finish를 호출하는 방식
 */
UCLASS()
class POSISONFROG_API UCTimeCooldownSkillIconWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;

    /** 쿨타임 시작: MaxSeconds를 세팅하고 즉시 UI 반영 */
    UFUNCTION(BlueprintCallable, Category="Cooldown")
    void StartCooldown(float InMaxSeconds);

    /** 남은 시간(초) 기준 업데이트: 내부에서 정규화(0~1) 후 Progress에 반영 */
    UFUNCTION(BlueprintCallable, Category="Cooldown")
    void UpdateCooldownByRemaining(float RemainingSeconds);

    /** 경과 시간(초) 기준 업데이트: Remaining = Max - Elapsed 후 위와 동일 */
    UFUNCTION(BlueprintCallable, Category="Cooldown")
    void UpdateCooldownByElapsed(float ElapsedSeconds);

    /** 쿨타임 종료: 바/아이콘 복구 & SFX/VFX 트리거 */
    UFUNCTION(BlueprintCallable, Category="Cooldown")
    void FinishCooldown(bool bPlayEffects = true);

    /** 차단/쿨다운 중 사용 불가 등의 피드백 */
    UFUNCTION(BlueprintCallable, Category="Cooldown")
    void NotifyCooldownBlocked();

    UFUNCTION(BlueprintPure, Category="Cooldown")
    bool IsCoolingDown() const { return bIsCoolDown; }

    UFUNCTION(BlueprintPure, Category="Cooldown")
    float GetMaxCooldown() const { return MaxCooldownSeconds; }

    UFUNCTION(BlueprintPure, Category="Cooldown")
    float GetLastPercent() const { return LastPercent; }

protected:
    /** 위젯 연결 (UMG에서 동일 이름으로 Bind) */
    UPROPERTY(meta=(BindWidget))
    TObjectPtr<UImage> SkillIcon = nullptr;

    UPROPERTY(meta=(BindWidget))
    TObjectPtr<UProgressBar> SkillBar = nullptr;

    /** (선택) 애니메이션 – UMG에 있으면 자동 바인딩됨 */
    UPROPERTY(Transient, meta=(BindWidgetAnimOptional))
    UWidgetAnimation* Anim_CooldownStart = nullptr;

    UPROPERTY(Transient, meta=(BindWidgetAnimOptional))
    UWidgetAnimation* Anim_CooldownTick = nullptr;

    UPROPERTY(Transient, meta=(BindWidgetAnimOptional))
    UWidgetAnimation* Anim_CooldownFinish = nullptr;

    /** 사운드/이펙트 자산 (Anim 전용 메타가 아니라 EditDefaultsOnly로 보관) */
    UPROPERTY(EditDefaultsOnly, Category="VFX/SFX")
    TObjectPtr<USoundBase> SFX_CooldownFinished = nullptr;

    UPROPERTY(EditDefaultsOnly, Category="VFX/SFX")
    TObjectPtr<USoundBase> SFX_CooldownBlocked = nullptr;

    UPROPERTY(EditDefaultsOnly, Category="VFX/SFX")
    TObjectPtr<UNiagaraSystem> VFX_CooldownFinishedOnPlayer = nullptr;

    /** 쿨타임 표시 스타일 */
    UPROPERTY(EditAnywhere, Category="Appearance")
    FLinearColor IconTint_Normal = FLinearColor(1.f, 1.f, 1.f, 1.f);

    UPROPERTY(EditAnywhere, Category="Appearance")
    FLinearColor IconTint_OnCooldown = FLinearColor(0.15f, 0.15f, 0.15f, 1.f);

    /** 내부 상태 */
    UPROPERTY(VisibleInstanceOnly, Category="Cooldown")
    bool bIsCoolDown = false;

    UPROPERTY(VisibleInstanceOnly, Category="Cooldown", meta=(ClampMin="0.0"))
    float MaxCooldownSeconds = 0.f;

    /** 마지막으로 UI에 반영한 Percent (0~1) – 불필요한 갱신/틴트 재설정 방지 */
    UPROPERTY(VisibleInstanceOnly, Category="Cooldown")
    float LastPercent = 0.f;

private:
    /** 0~1 정규화 + 클램프 */
    static float Normalize01(float Value, float Max);

    /** UI(바/아이콘) 갱신 – 내부에서 변경 감지 후 최소 갱신 */
    void RefreshUI(float NormalizedPercent);

    /** SFX/VFX/Anim 헬퍼 */
    void PlaySfx(USoundBase* SFX);
    void PlayFinishEffectsOnPlayer();
};

