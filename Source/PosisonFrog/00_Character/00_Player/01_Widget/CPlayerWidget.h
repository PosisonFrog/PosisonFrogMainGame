#pragma once

#include "CoreMinimal.h"
#include "00_Character/02_Component/00_PlayerComponent/ComboStackComponent.h"
#include "Blueprint/UserWidget.h"
#include "CPlayerWidget.generated.h"

class UMediaTexture;
class UMediaSource;
class UMediaPlayer;
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
    /*UFUNCTION(BlueprintCallable, Category = "PF|HUD")
    void UpdateUltimateImage(float Current, float Max);*/

    // 스핀 스킬 - 게이지 방식
    //UFUNCTION(BlueprintCallable, Category = "PF|HUD")
    //void UpdateFuryStacksBar(int32 NewStacks, int32 MaxStacks);

    // 스핀 스킬 - 이미지 활성화 방식
    UFUNCTION(BlueprintCallable, Category = "PF|HUD")
    void UpdateFuryStacksImage(int32 NewStacks, int32 MaxStacks);

    // 콤보 스택 (CSC)
    UFUNCTION(BlueprintCallable, Category = "PF|HUD")
    void UpdateComboStack(int32 NewCSC);

    // 콤보 랭크
    UFUNCTION(BlueprintCallable, Category = "PF|HUD")
    void UpdateComboRank(EComboRank OldRank, EComboRank NewRank);

    // 궁극기 UI 연출 부분
    UFUNCTION(BlueprintCallable, Category = "PF|HUD")
    void OnUltimateActivated();

    UFUNCTION(BlueprintCallable, Category = "PF|HUD")
    void OnUltimateDeactivated();
    
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

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

    UPROPERTY(meta = (BindWidgetOptional))
    UImage* UltAnimationImage = nullptr;
    
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
    // 해당 내용들은 나중에 게이지 사용하면 주석 풀기
    /*UPROPERTY(EditAnywhere, Category = "PF|HUD|Ultimate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float UltRank1Threshold = 0.2f;
    UPROPERTY(EditAnywhere, Category = "PF|HUD|Ultimate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float UltRank2Threshold = 0.4f;
    UPROPERTY(EditAnywhere, Category = "PF|HUD|Ultimate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float UltRank3Threshold = 0.6f;
    UPROPERTY(EditAnywhere, Category = "PF|HUD|Ultimate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float UltRank4Threshold = 0.8f;
    UPROPERTY(EditAnywhere, Category = "PF|HUD|Ultimate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float UltRank5Threshold = 1.0f;*/

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

    // ─────────── 궁극기 연출 설정 ───────────
    // Media Player
    UPROPERTY(EditAnywhere, Category = "PF|HUD|Ultimate|Animation")
    TObjectPtr<UMediaSource> UltAnimationMediaSource = nullptr;

    UPROPERTY(EditAnywhere, Category = "PF|HUD|Ultimate|Animation")
    TObjectPtr<UMediaPlayer> UltMediaPlayer = nullptr;

    UPROPERTY(EditAnywhere, Category = "PF|HUD|Ultimate|Animation")
    TObjectPtr<UMediaTexture> UltMediaTexture = nullptr;

    UPROPERTY(EditAnywhere, Category = "PF|HUD|Ultimate|Animation")
    float HpBarChangeDelay = 0.9f;
    
    // HP Bar 이미지 교체용
    UPROPERTY(EditAnywhere, Category = "PF|HUD|Ultimate|HPBar")
    TObjectPtr<UTexture2D> HPBarImage_Normal = nullptr;
    UPROPERTY(EditAnywhere, Category = "PF|HUD|Ultimate|HPBar")
    TObjectPtr<UTexture2D> HPBarBackgroundImage_Normal = nullptr;

    UPROPERTY(EditAnywhere, Category = "PF|HUD|Ultimate|HPBar")
    TObjectPtr<UTexture2D> HPBarImage_Ultimate = nullptr;
    UPROPERTY(EditAnywhere, Category = "PF|HUD|Ultimate|HPBar")
    TObjectPtr<UTexture2D> HPBarBackgroundImage_Ultimate = nullptr;
    
    UPROPERTY(Transient)
    FSlateBrush OriginalHpBarBrush;

    UPROPERTY(Transient)
    FSlateBrush OriginalHpBarBackgroundBrush;
    
    // HP Bar 원래 위치/사이즈 저장
    UPROPERTY(Transient)
    FVector2D OriginalHpBarPosition = FVector2D::ZeroVector;

    UPROPERTY(Transient)
    FVector2D OriginalHpBarSize = FVector2D::ZeroVector;

    UPROPERTY(Transient)
    bool bOriginalHpBarTransformSaved = false;

    UPROPERTY(EditAnywhere, Category = "PF|HUD|Ultimate|HPBar")
    FVector2D UltimateHpBarPosition = FVector2D(274.f, 12.0f);

    UPROPERTY(EditAnywhere, Category = "PF|HUD|Ultimate|HPBar")
    FVector2D UltimateHpBarSize = FVector2D(0.0f, 94.f);

    // HP Bar Anchor 설정
    UPROPERTY(Transient)
    FAnchors OriginalHpBarAnchors;

    UPROPERTY(Transient)
    bool bOriginalHpBarAnchorsSaved = false;

    // 일반 상태: 왼쪽 위 앵커 (0,0,0,0)
    UPROPERTY(EditAnywhere, Category = "PF|HUD|Ultimate|HPBar")
    FAnchors NormalHpBarAnchors = FAnchors(0.f, 0.f, 0.f, 0.f);

    // 궁극기 상태: 상단 면 전체 앵커 (0,0,1,0)
    UPROPERTY(EditAnywhere, Category = "PF|HUD|Ultimate|HPBar")
    FAnchors UltimateHpBarAnchors = FAnchors(0.f, 0.f, 1.f, 0.f);

    FTimerHandle TimerHandle_HpBarChange;
    FTimerHandle TimerHandle_HpLerp;
    FTimerHandle TimerHandle_OverHealLerp;
    
    UPROPERTY(Transient)
    bool bUltimateActive = false;
    
private:
    void StopDashFX();

    void UpdateFuryStacksImages(int32 CurrentStacks);

    // ─────────── 궁극기 연출 함수 ───────────
    // 궁극기 UI 업데이트 관련 함수
    void HideSpinUltImages();
    // 나중에 궁극기 게이지로 변동되면 사용하기
    //void UpdateUltimateRankImages(float Ratio);
    FLinearColor GetColorForRank(EComboRank Rank) const;
    FText GetTextForRank(EComboRank Rank) const;

    // 궁극기 애니메이션 관련
    void PlayUltimateAnimation();
    UFUNCTION() void OnUltimateAnimationFinished();

    // HP Bar 변경 (지연 후 호출)
    UFUNCTION() void ApplyUltimateHpBarChanges();
    
    void SetHpBarImage(UTexture2D* NewTexture);
    void RestoreHpBarImage();

    // HP Bar 위치/사이즈 변경 헬퍼
    void SaveOriginalHpBarTransform();
    void SetHpBarTransform(const FVector2D& Position, const FVector2D& Size);
    void SaveOriginalHpBarAnchors();
    void SetHpBarAnchors(const FAnchors& Anchors);

    // ─────────── 체력 부드럽게 변환하기 위한 Lerp 시스템 ───────────
    // Lerp 업데이트 함수
    UFUNCTION() void UpdateHpLerp();
    UFUNCTION() void UpdateOverHealLerp();
    
    // HP Bar
    UPROPERTY(Transient)
    float CurrentDisplayHpRatio = 1.0f; // 화면에 표시되는 비율
    
    UPROPERTY(Transient)
    float TargetHpRatio = 1.0f; // 목표 비율
    
    UPROPERTY(EditAnywhere, Category = "PF|HUD|Lerp", meta = (ClampMin = "0.1"))
    float HpLerpSpeed = 5.0f; // 보간 속도

    // OverHeal Bar
    UPROPERTY(Transient)
    float CurrentDisplayOverHealRatio = 0.0f;
    
    UPROPERTY(Transient)
    float TargetOverHealRatio = 0.0f;
    
    UPROPERTY(EditAnywhere, Category = "PF|HUD|Lerp", meta = (ClampMin = "0.1"))
    float OverHealLerpSpeed = 5.0f;

    
    // 내부 헬퍼
    static float SafeRatio(float Num, float Denom);
    static FText SecsTextOneDecimal(float Seconds);
};
