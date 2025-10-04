#include "CPlayerWidget.h"

#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

namespace
{
    // 안전한 클램프
    FORCEINLINE float Clamp01(float V) { return FMath::Clamp(V, 0.f, 1.f); }
}

void UCPlayerWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    // 초기 색상/상태 지정(위젯이 있을 때만)
    if (HealthBar)
        HealthBar->SetFillColorAndOpacity(HpColor_Normal);

    if (DashCooldownBar)
        DashCooldownBar->SetFillColorAndOpacity(DashColor_Ready);
}

// 초기 상태는 READY로 세팅(에디터 미리보기/런타임 모두 안전)
void UCPlayerWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 위젯 생성 직후 기본 텍스트/수치 정리
    if (HealthBar)      HealthBar->SetPercent(1.0f);
    //if (HealthText)     HealthText->SetText(FText::FromString(TEXT("HP 100%")));

    SetDashReady(); // 대시는 처음엔 Ready 상태로
}
void UCPlayerWidget::UpdateHpBar(float Current, float Max)
{
    const float Ratio = SafeRatio(Current, Max);  // 0~1
    if (HealthBar)
    {
        HealthBar->SetPercent(Ratio);
        // 색상: 위험 임계치 이하면 Danger 색
        const FLinearColor UseColor = (Ratio <= HpDangerThreshold) ? HpColor_Danger : HpColor_Normal;
        HealthBar->SetFillColorAndOpacity(UseColor);
    }

    /*if (HealthText)
    {
        const int32 ICur = FMath::Max(0, FMath::RoundToInt(Current));
        const int32 IMax = FMath::Max(1, FMath::RoundToInt(Max));
        HealthText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), ICur, IMax)));
    }*/
}


void UCPlayerWidget::UpdateDashCooldown(float Remaining, float Total)
{
    Remaining = FMath::Max(0.f, Remaining);
    Total = FMath::Max(0.001f, Total);

    // Percent는 "남은 시간 비율"로 표시(1 -> 막 시작, 0 -> 준비 완료)
    const float P = Clamp01(Remaining / Total);

    if (DashCooldownBar)
    {
        DashCooldownBar->SetPercent(P);
        DashCooldownBar->SetFillColorAndOpacity(P > 0.f ? DashColor_Cooldown : DashColor_Ready);
    }

    /*if (DashText)
    {
        if (P > 0.f)
        {
            // 남은 시간 소수 1자리
            const FText T = SecsTextOneDecimal(Remaining);
            DashText->SetText(FText::Format(NSLOCTEXT("PF", "DashRemainFmt", "Dash {0}s"), T));
        }
        else
        {
            DashText->SetText(NSLOCTEXT("PF", "DashReady", "Dash READY"));
        }
    }*/
}

void UCPlayerWidget::SetDashReady()
{
    if (DashCooldownBar)
    {
        DashCooldownBar->SetPercent(0.f);
        DashCooldownBar->SetFillColorAndOpacity(DashColor_Ready);
    }
    /*if (DashText)
    {
        DashText->SetText(NSLOCTEXT("PF", "DashReady_Init", "Dash READY"));
    }*/

    if (DashFXImage)
    {
        DashFXImage->SetVisibility(ESlateVisibility::Hidden);
    }
}

void UCPlayerWidget::PlayDashFX(float Duration)
{
    if (DashFXImage)
        DashFXImage->SetVisibility(ESlateVisibility::HitTestInvisible);

    GetWorld()->GetTimerManager().ClearTimer(TimerHandle_DashFX);
    if (Duration > 0.0f)
    {
        GetWorld()->GetTimerManager().SetTimer(TimerHandle_DashFX, this, &UCPlayerWidget::StopDashFX, Duration, false);
    }
}

void UCPlayerWidget::StopDashFX()
{
    if (DashFXImage)
        DashFXImage->SetVisibility(ESlateVisibility::Hidden);
}

float UCPlayerWidget::SafeRatio(float Num, float Denom)
{
    if (Denom <= KINDA_SMALL_NUMBER) return 0.f;
    return FMath::Clamp(Num / Denom, 0.f, 1.f);
}

FText UCPlayerWidget::SecsTextOneDecimal(float Seconds)
{
    // "0.0" 형식으로 1자리 소수 출력
    return FText::FromString(FString::Printf(TEXT("%.1f"), Seconds));
}
