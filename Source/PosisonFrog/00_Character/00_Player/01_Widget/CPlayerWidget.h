#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CPlayerWidget.generated.h"

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
    // 체력/대시 공개 API (캐릭터에서 호출)
    UFUNCTION(BlueprintCallable, Category = "PF|HUD")
    void UpdateHpBar(float Current, float Max);

    UFUNCTION(BlueprintCallable, Category = "PF|HUD")
    void UpdateDashCooldown(float Remaining, float Total);

    UFUNCTION(BlueprintCallable, Category = "PF|HUD")
    void SetDashReady();

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;

private:
    // ===== UMG 바인딩(위젯 이름만 맞추면 블루프린트 스크립트 불필요) =====
    // (BindWidgetOptional: 누락되어 있어도 크래시 방지)
    UPROPERTY(meta = (BindWidgetOptional))
    UProgressBar* HealthBar = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* HealthText = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UProgressBar* DashCooldownBar = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* DashText = nullptr;

    // ===== 표시/연출 관련 기본값 =====
    UPROPERTY(EditAnywhere, Category = "PF|HUD|HP")
    float HpDangerThreshold = 0.25f; // 25% 이하면 위험색

    UPROPERTY(EditAnywhere, Category = "PF|HUD|HP")
    FLinearColor HpColor_Normal = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
    //FLinearColor HpColor_Normal = FLinearColor(0.10f, 0.85f, 0.20f, 1.0f);

    UPROPERTY(EditAnywhere, Category = "PF|HUD|HP")
    FLinearColor HpColor_Danger = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
    //FLinearColor HpColor_Danger = FLinearColor(0.90f, 0.10f, 0.10f, 1.0f);

    UPROPERTY(EditAnywhere, Category = "PF|HUD|Dash")
    FLinearColor DashColor_Cooldown = FLinearColor(0.20f, 0.45f, 1.0f, 1.0f);

    UPROPERTY(EditAnywhere, Category = "PF|HUD|Dash")
    FLinearColor DashColor_Ready = FLinearColor(0.95f, 0.80f, 0.15f, 1.0f);

private:
    // 내부 헬퍼
    static float SafeRatio(float Num, float Denom);
    static FText SecsTextOneDecimal(float Seconds);
};

