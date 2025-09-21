#include "CPlayerWidget.h"

#include "CPlayerHpBarWidget.h"
#include "CTimeCooldownSkillIconWidget.h"
#include "CUltimateSkillIconWidget.h"
#include "Components/TextBlock.h"

// 초기 상태는 READY로 세팅(에디터 미리보기/런타임 모두 안전)
void UCPlayerWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 숫자 텍스트 초기화
    if (DashCooldownText)
    {
        if (bShowDashText)
        {
            DashCooldownText->SetText(ReadyText);
            DashCooldownText->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
        else
        {
            DashCooldownText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    // 아이콘 초기화(있을 경우)
   /* if (WBP_DashSkillIconUIWidget)
    {
        // 프로젝트 구현에 따라: 0(꽉참) 또는 Finish로 READY 표현
        WBP_DashSkillIconUIWidget->FinishCoolDown();
    }*/
    if (WBP_DashSkillIconWidget)
    {
        WBP_DashSkillIconWidget->FinishCooldown();
    }
}

void UCPlayerWidget::UpdateHpBar(float Current, float Max)
{
    if (WBP_PlayerHpBar)
    {
        WBP_PlayerHpBar->UpdateHp(Current, Max);
    }
}

void UCPlayerWidget::UpdateDashCooldown(float RemainingSeconds, float TotalSeconds)
{
    // 총 시간 0 or 음수 → 쿨다운 개념이 없거나 즉시 READY 처리
    if (TotalSeconds <= 0.f)
    {
        SetDashReady();
        return;
    }

    const float ClampedRemaining = FMath::Max(RemainingSeconds, 0.f);
    const float RatioLeft = FMath::Clamp(ClampedRemaining / TotalSeconds, 0.f, 1.f);
    const float ElapsedRatio = 1.f - RatioLeft; // 0→1로 차오르는 게 일반적

    // 1) 텍스트 갱신
    if (DashCooldownText)
    {
        if (ClampedRemaining > 0.f)
        {
            ShowDashTextSeconds_Internal(ClampedRemaining);
        }
        else
        {
            ShowDashTextReady_Internal();
        }
    }

    // 2) 아이콘(프로그레스/원형 타이머 등) 갱신
    UpdateDashProgress_Internal(ElapsedRatio, TotalSeconds);

    // 3) 남은 시간이 0 이하이면 READY 상태 보장
    if (ClampedRemaining <= 0.f)
    {
        SetDashReady(); // 아이콘/텍스트 모두 READY 표기로 통일
    }
}

void UCPlayerWidget::SetDashReady()
{
    
    // 텍스트 READY
    if (DashCooldownText)
    {
        if (bShowDashText)
        {
            DashCooldownText->SetText(ReadyText);
            DashCooldownText->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
        else
        {
            DashCooldownText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    // 아이콘 READY 처리
    
    if (WBP_DashSkillIconWidget)
    {
        WBP_DashSkillIconWidget->FinishCooldown();
    }
}

void UCPlayerWidget::SetUltimatePoints(float UltimateCurrentPoints, float UltimateMaxPoints)
{
    if (!WBP_UltimateSkillIconWidget || UltimateMaxPoints <= 0.f)
        return;

    const float Ratio = FMath::Clamp(UltimateCurrentPoints / UltimateMaxPoints, 0.f, 1.f);
    WBP_UltimateSkillIconWidget->SetRatio(Ratio);
}

// ───────────── 내부 헬퍼들 ─────────────

void UCPlayerWidget::UpdateDashProgress_Internal(float ElapsedRatio, float TotalSeconds)
{
    // 두 가지 타입의 아이콘 위젯을 모두 지원(있으면 각각 갱신)
 
    // 쿨다운이 시작되지 않았다면 시작
    if (!WBP_DashSkillIconWidget->IsCoolingDown())
    {
        WBP_DashSkillIconWidget->StartCooldown(TotalSeconds);
    }
        
    // 경과된 시간 계산하여 업데이트
    float ElapsedSeconds = ElapsedRatio * TotalSeconds;
    WBP_DashSkillIconWidget->UpdateCooldownByElapsed(ElapsedSeconds);

}

void UCPlayerWidget::ShowDashTextSeconds_Internal(float RemainingSeconds)
{
    if (!DashCooldownText) return;

    if (!bShowDashText)
    {
        DashCooldownText->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    // 소수 1자리 “X.Xs” 포맷
    // (로케일 반영을 위해 FText 포맷 사용)
    FNumberFormattingOptions Opt;
    Opt.SetMinimumFractionalDigits(1);
    Opt.SetMaximumFractionalDigits(1);

    const FText SecText = FText::AsNumber((double)RemainingSeconds, &Opt);
    DashCooldownText->SetText(FText::Format(FText::FromString(TEXT("{0}s")), SecText));
    DashCooldownText->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UCPlayerWidget::ShowDashTextReady_Internal()
{
    if (!DashCooldownText) return;

    if (bShowDashText)
    {
        DashCooldownText->SetText(ReadyText);
        DashCooldownText->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    else
    {
        DashCooldownText->SetVisibility(ESlateVisibility::Collapsed);
    }
}

