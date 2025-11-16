#pragma once

#include "CoreMinimal.h"
#include "00_Character/02_Component/00_PlayerComponent/ComboStackComponent.h"
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
    // ─────────── 공개 API (캐릭터에서 호출) ───────────
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

    // 궁극기 - 게이지 방식
    UFUNCTION(BlueprintCallable, Category = "PF|HUD")
    void UpdateUltimateBar(float Current, float Max);

    // 궁극기 - 이미지 활성화 방식
    UFUNCTION(BlueprintCallable, Category = "PF|HUD")
    void UpdateUltimateImage(float Current, float Max);

    // 스핀 스킬 - 게이지 방식
    UFUNCTION(BlueprintCallable, Category = "PF|HUD")
    void UpdateFuryStacksBar(int32 NewStacks, int32 MaxStacks);

    UFUNCTION(BlueprintCallable, Category = "PF|HUD")
    void UpdateFuryStacksImage(int32 NewStacks, int32 MaxStacks);

    // 콤보 스택 (CSC)
    UFUNCTION(BlueprintCallable, Category = "PF|HUD")
    void UpdateComboStack(int32 NewCSC);

    // 콤보 랭크
    UFUNCTION(BlueprintCallable, Category = "PF|HUD")
    void UpdateComboRank(EComboRank OldRank, EComboRank NewRank);
    
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;

private:
    // ─────────── UMG 바인딩(위젯 이름만 맞추면 블루프린트 스크립트 불필요) ───────────
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
    UTextBlock* CSCComboCount = nullptr;

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
    
    // ─────────── 표시/연출 관련 기본값 ───────────
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

    // ─────────── Fury 스택 임계값 설정 ───────────
    UPROPERTY(EditAnywhere, Category = "PF|HUD|Fury", meta = (ClampMin = "0"))
    int32 FuryStack1Threshold = 1;
    
    UPROPERTY(EditAnywhere, Category = "PF|HUD|Fury", meta = (ClampMin = "0"))
    int32 FuryStack2Threshold = 3;
    
    UPROPERTY(EditAnywhere, Category = "PF|HUD|Fury", meta = (ClampMin = "0"))
    int32 FuryStack3Threshold = 7;
    
    UPROPERTY(EditAnywhere, Category = "PF|HUD|Fury", meta = (ClampMin = "0"))
    int32 FuryStack4Threshold = 10;

    // ─────────── Ultimate 게이지 임계값 설정 ───────────
    UPROPERTY(EditAnywhere, Category = "PF|HUD|Ultimate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float UltRank1Threshold = 0.2f;
    
    UPROPERTY(EditAnywhere, Category = "PF|HUD|Ultimate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float UltRank2Threshold = 0.4f;
    
    UPROPERTY(EditAnywhere, Category = "PF|HUD|Ultimate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float UltRank3Threshold = 0.6f;
    
    UPROPERTY(EditAnywhere, Category = "PF|HUD|Ultimate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float UltRank4Threshold = 0.8f;
    
    UPROPERTY(EditAnywhere, Category = "PF|HUD|Ultimate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float UltRank5Threshold = 1.0f;

    // ─────────── 콤보 랭크 색상 설정 ───────────
    UPROPERTY(EditAnywhere, Category = "PF|HUD|Combo")
    FLinearColor RankColor_D = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);

    UPROPERTY(EditAnywhere, Category = "PF|HUD|Combo")
    FLinearColor RankColor_C = FLinearColor(0.4f, 0.8f, 0.4f, 1.0f);

    UPROPERTY(EditAnywhere, Category = "PF|HUD|Combo")
    FLinearColor RankColor_B = FLinearColor(0.4f, 0.6f, 1.0f, 1.0f);

    UPROPERTY(EditAnywhere, Category = "PF|HUD|Combo")
    FLinearColor RankColor_A = FLinearColor(0.9f, 0.5f, 1.0f, 1.0f);

    UPROPERTY(EditAnywhere, Category = "PF|HUD|Combo")
    FLinearColor RankColor_S = FLinearColor(1.0f, 0.8f, 0.2f, 1.0f);

private:
    void StopDashFX();

    void HideSpinUltImages();

    void UpdateFuryStacksImages(int32 CurrentStacks);
    void UpdateUltimateRankImages(float Ratio);

    FLinearColor GetColorForRank(EComboRank Rank) const;
    FText GetTextForRank(EComboRank Rank) const;
    
    // 내부 헬퍼
    static float SafeRatio(float Num, float Denom);
    static FText SecsTextOneDecimal(float Seconds);

};

