// CSkillIconBaseWidget.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CSkillIconBaseWidget.generated.h"

class UProgressBar;
class UImage;
class USoundBase;
class UNiagaraSystem;

/** 쿨타임 종료 알림 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSkillCooldownFinished);

/**
 * 스킬 아이콘 공용 위젯
 * - 쿨타임 바(ProgressBar) 표시
 * - 쿨타임 종료/차단 SFX, 애니, (옵션) VFX
 * - TryStartCooldown()로 시작, 종료 시 자동 Ready 연출
 */
UCLASS()
class POSISONFROG_API UCSkillIconBaseWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** 쿨타임 아닌 상태에서만 시작. 이미 쿨타임 중이면 Block 연출 후 false 반환 */
    UFUNCTION(BlueprintCallable, Category="Skill|Cooldown")
    bool TryStartCooldown(float InTotalSeconds);

    /** 강제로 쿨타임 시작(테스트/디버그용). 이미 진행 중이어도 덮어씀 */
    UFUNCTION(BlueprintCallable, Category="Skill|Cooldown")
    void StartCooldown(float InTotalSeconds);

    /** 쿨타임 진행 여부 */
    UFUNCTION(BlueprintPure, Category="Skill|Cooldown")
    bool IsCoolingDown() const { return bInCooldown; }

    /** 스킬 버튼이 눌렸음을 알림(쿨타임이면 Block 연출만 수행) */
    UFUNCTION(BlueprintCallable, Category="Skill|Input")
    void NotifySkillPressed();

    /** 아이콘 텍스처 설정(선택) */
    UFUNCTION(BlueprintCallable, Category="Skill|Visual")
    void SetIcon(UTexture2D* InTex);

    /** 쿨타임 종료 브로드캐스트(블루프린트에서도 바인딩 가능) */
    UPROPERTY(BlueprintAssignable, Category="Skill|Events")
    FOnSkillCooldownFinished OnCooldownFinished;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // ─── 내부 유틸(연출) ───
    void PlaySkillReadySound();
    void PlayCoolTimeBlockedSound();
    void PlaySkillReadyAnim();
    void PlayCoolTimeBlockAnim();
    void PlaySkillReadyEffect();
    void PlayCoolTimeBlockedEffect();
    void UpdateProgressBar(float Normalized); // 0~1

private:
    void FinishCooldown_Internal(); // 종료 처리 공통

protected:
    // ─── 위젯 바인딩 ───
    /** 쿨타임 바(0~1). 기본은 “남은 비율”로 채우는 역채움 모드 */
    UPROPERTY(meta=(BindWidget))
    TObjectPtr<UProgressBar> SkillBar = nullptr;

    
    /** (선택) 아이콘 이미지 */
    UPROPERTY(meta=(BindWidgetOptional))
    TObjectPtr<UImage> SkillIcon = nullptr;

    /** UMG 애니(선택): 쿨타임 종료 시 */
    UPROPERTY(Transient, meta=(BindWidgetAnimOptional))
    TObjectPtr<UWidgetAnimation> Anim_SkillReady = nullptr;

    /** UMG 애니(선택): 쿨타임 중 차단 시 */
    UPROPERTY(Transient, meta=(BindWidgetAnimOptional))
    TObjectPtr<UWidgetAnimation> Anim_CoolBlocked = nullptr;

    // ─── 오디오/VFX ───
    UPROPERTY(EditAnywhere, Category="Skill|SFX")
    TObjectPtr<USoundBase> SFX_SkillReady = nullptr;

    UPROPERTY(EditAnywhere, Category="Skill|SFX")
    TObjectPtr<USoundBase> SFX_CoolTimeBlocked = nullptr;

    /** (선택) 쿨타임 종료 시 플레이어 발밑 VFX */
    UPROPERTY(EditAnywhere, Category="Skill|VFX")
    TObjectPtr<UNiagaraSystem> VFX_CoolTimeFinishedOnPlayer = nullptr;

    /** (선택) 쿨타임 차단 시 효과 */
    UPROPERTY(EditAnywhere, Category="Skill|VFX")
    TObjectPtr<UNiagaraSystem> VFX_CoolBlocked = nullptr;

    // ─── 동작 파라미터 ───
    /** 진행 바를 역채움으로 쓸지(기본: 남은시간/전체 = 1.0→0.0) */
    UPROPERTY(EditAnywhere, Category="Skill|Visual")
    bool bInverseFill = true;

    /** 쿨타임 종료 시 바를 어느 값으로 둘지(역채움이면 0.0, 정채움이면 1.0 권장) */
    UPROPERTY(EditAnywhere, Category="Skill|Visual", meta=(ClampMin="0.0", ClampMax="1.0"))
    float ReadyFillValue = 0.0f;

    /** 쿨타임 총 시간/남은 시간(초) */
    UPROPERTY(VisibleInstanceOnly, Category="Skill|Cooldown")
    float CooldownTotal = 0.f;

    UPROPERTY(VisibleInstanceOnly, Category="Skill|Cooldown")
    float CooldownRemaining = 0.f;

    /** 시간 계산용(월드 시간 기준) */
    float CooldownEndTime = 0.f;

    /** 진행 중 플래그 */
    bool bInCooldown = false;
};

