#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CPlayerWidget.generated.h"

class UImage;
class UTextBlock;
class UProgressBar;  
class UCPlayerHpBarWidget;

/**
 * 플레이어 HUD (체력/대시 쿨다운)
 * - ACPlayerCharacter에서 직접 호출하여 수치 갱신
 * - UMG는 최소 구성(ProgressBar + Text), 로직은 전부 C++
 */
UCLASS()
class POSISONFROG_API UCPlayerWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // --- 공개 API (캐릭터에서 호출) ---
    // 체력
    UFUNCTION(BlueprintCallable, Category = "PF|HUD")
    void UpdateHpBar(float Current, float Max);

    // 오버힐
    UFUNCTION(BlueprintCallable, Category = "PF|HUD")
    void UpdateOverHealHPBar(float CurrentOverHeal, float MaxOverHeal);
    
    // 대쉬
    UFUNCTION(BlueprintCallable, Category = "PF|HUD")
    void UpdateDashCooldown(float Remaining, float Total);
    
    UFUNCTION(BlueprintCallable, Category = "PF|HUD")
    void SetDashReady();

    UFUNCTION(BlueprintCallable, Category = "PF|HUD")
    void PlayDashFX(float Duration);

    // 궁극기
    UFUNCTION(BlueprintCallable, Category = "PF|HUD")
    void UpdateUltimateBar(float Current, float Max);

    // 스핀 스킬
    UFUNCTION(BlueprintCallable, Category = "PF|HUD")
    void UpdateFuryStacks(int32 NewStacks, int32 MaxStacks);
    
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;

private:
    // ===== UMG 바인딩(위젯 이름만 맞추면 블루프린트 스크립트 불필요) =====
    // (BindWidgetOptional: 누락되어 있어도 크래시 방지)
    // HP
    UPROPERTY(meta = (BindWidgetOptional))
    UProgressBar* HealthBar = nullptr;


    // OverHealHealth
    UPROPERTY(meta = (BindWidgetOptional))
    UProgressBar* OverHealHpBar = nullptr;
    
    // Dash
    UPROPERTY(meta = (BindWidgetOptional))
    UProgressBar* DashCooldownBar = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UImage* DashFXImage = nullptr;

    FTimerHandle TimerHandle_DashFX;
    
    // Ultimate
    UPROPERTY(meta = (BindWidgetOptional))
    UImage* Ult_HPIcon = nullptr;
    
    UPROPERTY(meta = (BindWidgetOptional))
    UProgressBar* UltimateBar = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* SCSComboCount = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UImage* UltRank_1 = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UImage* UltRank_2 = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UImage* UltRank_3 = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UImage* UltRank_4 = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UImage* UltRank_5 = nullptr;
    
    // 스핀
    UPROPERTY(meta = (BindWidgetOptional))
    UProgressBar* FuryGaugeBar = nullptr;
    
    UPROPERTY(meta = (BindWidgetOptional))
    UImage* SpinStack_1 = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UImage* SpinStack_2 = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UImage* SpinStack_3 = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UImage* SpinStack_4 = nullptr;
    
    // ===== 표시/연출 관련 기본값 =====
    UPROPERTY(EditAnywhere, Category = "PF|HUD|HP")
    float HpDangerThreshold = 0.25f; // 25% 이하면 위험색

    UPROPERTY(EditAnywhere, Category = "PF|HUD|HP")
    FLinearColor HpColor_Normal = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
    //FLinearColor HpColor_Normal = FLinearColor(0.10f, 0.85f, 0.20f, 1.0f);

    UPROPERTY(EditAnywhere, Category = "PF|HUD|HP")
    FLinearColor HpColor_Danger = FLinearColor(0.90f, 0.10f, 0.10f, 1.0f);

    UPROPERTY(EditAnywhere, Category = "PF|HUD|Dash")
    FLinearColor DashColor_Cooldown = FLinearColor(0.20f, 0.45f, 1.0f, 1.0f);

    UPROPERTY(EditAnywhere, Category = "PF|HUD|Dash")
    FLinearColor DashColor_Ready = FLinearColor(0.95f, 0.80f, 0.15f, 1.0f);

private:
    void StopDashFX();
    
    // 내부 헬퍼
    static float SafeRatio(float Num, float Denom);
    static FText SecsTextOneDecimal(float Seconds);
};

