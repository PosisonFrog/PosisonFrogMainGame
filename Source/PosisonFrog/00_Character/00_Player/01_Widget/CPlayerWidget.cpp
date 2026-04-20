#include "CPlayerWidget.h"

#include "MediaPlayer.h"
#include "00_Character/01_Enemy/CEnemyBossCharacter.h"
#include "00_Character/02_Component/01_EnemyComponent/CEnemyHealthComponent.h"
#include "99_Util/CLog.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
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
    {
        HealthBar->SetFillColorAndOpacity(HpColor_Normal);

        const FProgressBarStyle& HealthBarStyle = HealthBar->GetWidgetStyle();
        OriginalHpBarBrush = HealthBarStyle.FillImage;
        OriginalHpBarBackgroundBrush = HealthBarStyle.BackgroundImage;

        SaveOriginalHpBarTransform();
        SaveOriginalHpBarAnchors();
    }

    if (DashCooldownBar)
        DashCooldownBar->SetFillColorAndOpacity(DashColor_Ready);

    if (UltMediaPlayer)
        UltMediaPlayer->OnEndReached.AddDynamic(this, &UCPlayerWidget::OnUltimateAnimationFinished);
}

// 초기 상태는 READY로 세팅(에디터 미리보기/런타임 모두 안전)
void UCPlayerWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 위젯 생성 직후 기본 텍스트/수치 정리
    if (HealthBar)      HealthBar->SetPercent(1.0f);
    if (UltimateBar)    UltimateBar->SetPercent(0.0f);
    if (OverHealHpBar)  OverHealHpBar->SetPercent(0.0f);
    
    SetDashReady(); // 대시는 처음엔 Ready 상태로
    
    if (FuryGaugeBar)
        FuryGaugeBar->SetPercent(0.0f);

    if (CSCComboCount)
    {
        CSCComboCount->SetText(GetTextForRank(EComboRank::D));
        CSCComboCount->SetColorAndOpacity(GetColorForRank(EComboRank::D));
    }

    if (UltAnimationImage)
        UltAnimationImage->SetVisibility(ESlateVisibility::Hidden);

    HideBossHealthBar();
    
    HideSpinUltImages();
}

void UCPlayerWidget::NativeDestruct()
{
    if (UltMediaPlayer)
    {
        UltMediaPlayer->Close();
        UltMediaTexture = nullptr;
    }

    UnsubscribeFromBoss();
    
    Super::NativeDestruct();
}

void UCPlayerWidget::UpdateHpBar(float Current, float Max)
{
    /*const float Ratio = SafeRatio(Current, Max);  // 0~1
    if (HealthBar)
    {
        HealthBar->SetPercent(Ratio);
        // 색상: 위험 임계치 이하면 Danger 색
        const FLinearColor UseColor = (Ratio <= HpDangerThreshold) ? HpColor_Danger : HpColor_Normal;
        HealthBar->SetFillColorAndOpacity(UseColor);
    }*/

    // Lerp 목표값 설정
    TargetHpRatio = SafeRatio(Current, Max);
    
    // Timer가 이미 실행 중이 아니면 시작
    if (!GetWorld()->GetTimerManager().IsTimerActive(TimerHandle_HpLerp))
    {
        GetWorld()->GetTimerManager().SetTimer(
            TimerHandle_HpLerp,
            this,
            &UCPlayerWidget::UpdateHpLerp,
            0.05f,    // 20hz
            true     // Loop
        );
    }
}

void UCPlayerWidget::UpdateOverHealHPBar(float CurrentOverHeal, float MaxOverHeal)
{
    /*if (!OverHealHpBar)
        return;

    const float Ratio = SafeRatio(CurrentOverHeal, 100.0f);
    OverHealHpBar->SetPercent(Ratio);
    
    if (CurrentOverHeal > 0.1f)
    {
        OverHealHpBar->SetVisibility(ESlateVisibility::Visible);
    }
    else
    {
        OverHealHpBar->SetVisibility(ESlateVisibility::Hidden);
    }*/

    // Lerp 목표값 설정
    TargetOverHealRatio = SafeRatio(CurrentOverHeal, 100.0f);
    
    // Timer 시작 (실행 중이 아닐 때만)
    if (!GetWorld()->GetTimerManager().IsTimerActive(TimerHandle_OverHealLerp))
    {
        GetWorld()->GetTimerManager().SetTimer(
            TimerHandle_OverHealLerp,
            this,
            &UCPlayerWidget::UpdateOverHealLerp,
            0.05f,    // 20hz
            true     // Loop
        );
    }
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
}

void UCPlayerWidget::SetDashReady()
{
    if (DashCooldownBar)
    {
        DashCooldownBar->SetPercent(0.f);
        DashCooldownBar->SetFillColorAndOpacity(DashColor_Ready);
    }

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

void UCPlayerWidget::UpdateUltimateBar(float Current, float Max)
{
    if (UltimateBar)
    {
        const float Ratio = SafeRatio(Current, Max);
        UltimateBar->SetPercent(Ratio);
    }
}

/*void UCPlayerWidget::UpdateUltimateImage(float Current, float Max)
{
    const float Ratio = SafeRatio(Current, Max);

    UpdateUltimateRankImages(Ratio);
}*/

/*void UCPlayerWidget::UpdateFuryStacksBar(int32 NewStacks, int32 MaxStacks)
{
    if (!FuryGaugeBar)
        return;

    const float Ratio = static_cast<float>(NewStacks) / static_cast<float>(MaxStacks);
    FuryGaugeBar->SetPercent(Ratio);
}*/

void UCPlayerWidget::UpdateFuryStacksImage(int32 NewStacks, int32 MaxStacks)
{
    UpdateFuryStacksImages(NewStacks);
}

void UCPlayerWidget::UpdateComboStack(int32 NewCSC)
{
    if (!CSCComboCount)
        return;

    // CSC 카운트 표시
    FString CountText = FString::Printf(TEXT("%d"), NewCSC);
    CSCComboCount->SetText(FText::FromString(CountText));
}

void UCPlayerWidget::UpdateComboRank(EComboRank OldRank, EComboRank NewRank)
{
    if (!CSCComboCount)
        return;

    // 랭크 색상 업데이트
    CSCComboCount->SetColorAndOpacity(GetColorForRank(NewRank));

    // UltRank 이미지들 업데이트 (D=0, C=1, B=2, A=3, S=4)
    uint8 RankValue = static_cast<uint8>(NewRank);
    if (UltRank_1)
        UltRank_1->SetVisibility(RankValue >= 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    if (UltRank_2)
        UltRank_2->SetVisibility(RankValue >= 1 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    if (UltRank_3)
        UltRank_3->SetVisibility(RankValue >= 2 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    if (UltRank_4)
        UltRank_4->SetVisibility(RankValue >= 3 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    if (UltRank_5)
        UltRank_5->SetVisibility(RankValue >= 4 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
}

void UCPlayerWidget::OnUltimateActivated()
{
    bUltimateActive = true;

    if (Ult_HPIcon)
        Ult_HPIcon->SetVisibility(ESlateVisibility::Visible);

    PlayUltimateAnimation();

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(TimerHandle_HpBarChange);
        GetWorld()->GetTimerManager().SetTimer(
            TimerHandle_HpBarChange,
            this,
            &UCPlayerWidget::ApplyUltimateHpBarChanges,
            HpBarChangeDelay,
            false);
    }
}

void UCPlayerWidget::OnUltimateDeactivated()
{
    bUltimateActive = false;

    if (Ult_HPIcon)
        Ult_HPIcon->SetVisibility(ESlateVisibility::Hidden);

    RestoreHpBarImage();

    if (UltAnimationImage)
    {
        UltAnimationImage->SetVisibility(ESlateVisibility::Hidden);
    }

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(TimerHandle_HpBarChange);
    }
}

void UCPlayerWidget::ShowBossHealthBar()
{
    if (Overlap_BossHp)
        Overlap_BossHp->SetVisibility(ESlateVisibility::Visible);
}

void UCPlayerWidget::HideBossHealthBar()
{
    if (Overlap_BossHp)
        Overlap_BossHp->SetVisibility(ESlateVisibility::Hidden);
}

void UCPlayerWidget::UpdateBossHealthBar(float Current, float Max)
{
    if (!BossHpBar)
        return;

    const float Ratio = SafeRatio(Current, Max);
    BossHpBar->SetPercent(Ratio);
}

void UCPlayerWidget::SubscribeToBoss(class ACEnemyBossCharacter* Boss)
{
    UnsubscribeFromBoss(); // 기존 구독 해제
    
    if (!Boss) return;
    SubscribedBoss = Boss;
    
    // 보스의 HealthComponent에서 OnHealthChanged 구독
    if (UCEnemyHealthComponent* HealthComp = SubscribedBoss->FindComponentByClass<UCEnemyHealthComponent>())
    {
        HealthComp->OnHealthChanged.AddDynamic(this, &UCPlayerWidget::UpdateBossHealthBar);
        UpdateBossHealthBar(HealthComp->GetHealth(), HealthComp->GetMaxHealth()); // 초기화
    }
}

void UCPlayerWidget::UnsubscribeFromBoss()
{
    if (!SubscribedBoss.IsValid()) return;
    
    if (UCEnemyHealthComponent* HealthComp = SubscribedBoss->FindComponentByClass<UCEnemyHealthComponent>())
    {
        HealthComp->OnHealthChanged.RemoveDynamic(this, &UCPlayerWidget::UpdateBossHealthBar);
    }
    
    SubscribedBoss.Reset();
}

void UCPlayerWidget::StopDashFX()
{
    if (DashFXImage)
        DashFXImage->SetVisibility(ESlateVisibility::Hidden);
}

void UCPlayerWidget::UpdateFuryStacksImages(int32 CurrentStacks)
{
    if (SpinStack_1)
    {
        const bool bShouldShow = CurrentStacks >= FuryStack1Threshold;
        SpinStack_1->SetVisibility(bShouldShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    }
    
    if (SpinStack_2)
    {
        const bool bShouldShow = CurrentStacks >= FuryStack2Threshold;
        SpinStack_2->SetVisibility(bShouldShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    }
    
    if (SpinStack_3)
    {
        const bool bShouldShow = CurrentStacks >= FuryStack3Threshold;
        SpinStack_3->SetVisibility(bShouldShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    }
    
    if (SpinStack_4)
    {
        const bool bShouldShow = CurrentStacks >= FuryStack4Threshold;
        SpinStack_4->SetVisibility(bShouldShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    }
}

void UCPlayerWidget::HideSpinUltImages()
{
    if (Ult_HPIcon)
        Ult_HPIcon->SetVisibility(ESlateVisibility::Hidden);
    
    if (UltRank_1)
        UltRank_1->SetVisibility(ESlateVisibility::Hidden);
    if (UltRank_2)
        UltRank_2->SetVisibility(ESlateVisibility::Hidden);
    if (UltRank_3)
        UltRank_3->SetVisibility(ESlateVisibility::Hidden);
    if (UltRank_4)
        UltRank_4->SetVisibility(ESlateVisibility::Hidden);
    if (UltRank_5)
        UltRank_5->SetVisibility(ESlateVisibility::Hidden);
    
    if (SpinStack_1)
        SpinStack_1->SetVisibility(ESlateVisibility::Hidden);
    if (SpinStack_2)
        SpinStack_2->SetVisibility(ESlateVisibility::Hidden);
    if (SpinStack_3)
        SpinStack_3->SetVisibility(ESlateVisibility::Hidden);
    if (SpinStack_4)
        SpinStack_4->SetVisibility(ESlateVisibility::Hidden);
}

/*void UCPlayerWidget::UpdateUltimateRankImages(float Ratio)
{
    if (UltRank_1)
    {
        const bool bShouldShow = Ratio >= UltRank1Threshold;
        UltRank_1->SetVisibility(bShouldShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    }
    
    if (UltRank_2)
    {
        const bool bShouldShow = Ratio >= UltRank2Threshold;
        UltRank_2->SetVisibility(bShouldShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    }
    
    if (UltRank_3)
    {
        const bool bShouldShow = Ratio >= UltRank3Threshold;
        UltRank_3->SetVisibility(bShouldShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    }
    
    if (UltRank_4)
    {
        const bool bShouldShow = Ratio >= UltRank4Threshold;
        UltRank_4->SetVisibility(bShouldShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    }
    
    if (UltRank_5)
    {
        const bool bShouldShow = Ratio >= UltRank5Threshold;
        UltRank_5->SetVisibility(bShouldShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    }
}*/

FLinearColor UCPlayerWidget::GetColorForRank(EComboRank Rank) const
{
    switch (Rank)
    {
    case EComboRank::D: return RankColor_D;
    case EComboRank::C: return RankColor_C;
    case EComboRank::B: return RankColor_B;
    case EComboRank::A: return RankColor_A;
    case EComboRank::S: return RankColor_S;
    default:            return FLinearColor::White;
    }
}

FText UCPlayerWidget::GetTextForRank(EComboRank Rank) const
{
    switch (Rank)
    {
    case EComboRank::D: return FText::FromString(TEXT("D"));
    case EComboRank::C: return FText::FromString(TEXT("C"));
    case EComboRank::B: return FText::FromString(TEXT("B"));
    case EComboRank::A: return FText::FromString(TEXT("A"));
    case EComboRank::S: return FText::FromString(TEXT("S"));
    default:            return FText::FromString(TEXT("?"));
    }
}

void UCPlayerWidget::PlayUltimateAnimation()
{
    if (!UltMediaPlayer || !UltMediaTexture || !UltAnimationMediaSource)
    {
        CLog::Log(TEXT("[UCPlayerWidget] Media Player/Texture/Source 설정 필요"));
        return;
    }

    // Media Player 재생
    if (UltMediaPlayer->OpenSource(UltAnimationMediaSource))
    {
        UltMediaPlayer->Rewind();
        UltMediaPlayer->Play();

        // 애니메이션 이미지 표시
        if (UltAnimationImage)
            UltAnimationImage->SetVisibility(ESlateVisibility::Visible);
        
        CLog::Log(TEXT("[UCPlayerWidget] 궁극기 애니메이션 재생 시작"));
    }
    else
    {
        CLog::Log(TEXT("[UCPlayerWidget] 궁극기 애니메이션 재생 실패"));
    }
}

void UCPlayerWidget::OnUltimateAnimationFinished()
{
    if (UltAnimationImage)
    {
        UltAnimationImage->SetVisibility(ESlateVisibility::Hidden);
    }
}

void UCPlayerWidget::ApplyUltimateHpBarChanges()
{
    if (!HPBarImage_Ultimate)
        return;

    UE_LOG(LogTemp, Log, TEXT("[UCPlayerWidget] HP 바 변경 적용 시작"));
    
    // HP 바 이미지, 위치, 사이즈, 앵커 모두 변경
    SetHpBarImage(HPBarImage_Ultimate);
}

void UCPlayerWidget::SetHpBarImage(UTexture2D* NewTexture)
{
    if (!HealthBar || !NewTexture)
        return;

    FProgressBarStyle NewStyle = HealthBar->GetWidgetStyle();
    
    // Fill 이미지 변경
    FSlateBrush NewFillBrush = NewStyle.FillImage;
    NewFillBrush.SetResourceObject(NewTexture);
    NewStyle.FillImage = NewFillBrush;
    
    // Background 이미지 변경
    if (HPBarBackgroundImage_Ultimate)
    {
        FSlateBrush NewBackgroundBrush = NewStyle.BackgroundImage;
        NewBackgroundBrush.SetResourceObject(HPBarBackgroundImage_Ultimate);
        NewStyle.BackgroundImage = NewBackgroundBrush;
    }
    
    // 스타일 적용
    HealthBar->SetWidgetStyle(NewStyle);

    SetHpBarAnchors(UltimateHpBarAnchors);

    // 위치/사이즈 변경
    SetHpBarTransform(UltimateHpBarPosition, UltimateHpBarSize);
}

void UCPlayerWidget::RestoreHpBarImage()
{
    if (!HealthBar)
        return;

    // 원래 스타일로 복구
    FProgressBarStyle OriginalStyle = HealthBar->GetWidgetStyle();
    OriginalStyle.FillImage = OriginalHpBarBrush;
    OriginalStyle.BackgroundImage = OriginalHpBarBackgroundBrush;
    
    HealthBar->SetWidgetStyle(OriginalStyle);

    if (bOriginalHpBarAnchorsSaved)
    {
        SetHpBarAnchors(OriginalHpBarAnchors);
    }

    // 원래 위치/사이즈로 복구
    if (bOriginalHpBarTransformSaved)
        SetHpBarTransform(OriginalHpBarPosition, OriginalHpBarSize);
}

void UCPlayerWidget::SaveOriginalHpBarTransform()
{
    if (!HealthBar || bOriginalHpBarTransformSaved)
        return;

    UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(HealthBar->Slot);
    if (CanvasSlot)
    {
        OriginalHpBarPosition = CanvasSlot->GetPosition();
        OriginalHpBarSize = CanvasSlot->GetSize();
        bOriginalHpBarTransformSaved = true;
    }
}

void UCPlayerWidget::SetHpBarTransform(const FVector2D& Position, const FVector2D& Size)
{
    if (!HealthBar)
        return;

    UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(HealthBar->Slot);
    if (CanvasSlot)
    {
        CanvasSlot->SetPosition(Position);
        CanvasSlot->SetSize(Size);
    }
}

void UCPlayerWidget::SaveOriginalHpBarAnchors()
{
    if (!HealthBar || bOriginalHpBarAnchorsSaved)
        return;

    UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(HealthBar->Slot);
    if (CanvasSlot)
    {
        OriginalHpBarAnchors = CanvasSlot->GetAnchors();
        bOriginalHpBarAnchorsSaved = true;
    }
}

void UCPlayerWidget::SetHpBarAnchors(const FAnchors& Anchors)
{
    if (!HealthBar)
        return;

    UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(HealthBar->Slot);
    if (CanvasSlot)
    {
        CanvasSlot->SetAnchors(Anchors);
    }
}

void UCPlayerWidget::UpdateHpLerp()
{
    const float DeltaTime = 0.05f;  // 20hz
    
    // 목표값에 도달했는지 확인
    if (FMath::IsNearlyEqual(CurrentDisplayHpRatio, TargetHpRatio, 0.001f))
    {
        // 정확히 목표값으로 설정하고 Timer 중지
        CurrentDisplayHpRatio = TargetHpRatio;
        
        if (HealthBar)
        {
            HealthBar->SetPercent(CurrentDisplayHpRatio);
            const FLinearColor UseColor = (CurrentDisplayHpRatio <= HpDangerThreshold) ? HpColor_Danger : HpColor_Normal;
            HealthBar->SetFillColorAndOpacity(UseColor);
        }
        
        GetWorld()->GetTimerManager().ClearTimer(TimerHandle_HpLerp);
        return;
    }
    
    // Lerp 계산
    CurrentDisplayHpRatio = FMath::FInterpTo(CurrentDisplayHpRatio, TargetHpRatio, DeltaTime, HpLerpSpeed);
    
    // UI 업데이트
    if (HealthBar)
    {
        HealthBar->SetPercent(CurrentDisplayHpRatio);
        const FLinearColor UseColor = (CurrentDisplayHpRatio <= HpDangerThreshold) ? HpColor_Danger : HpColor_Normal;
        HealthBar->SetFillColorAndOpacity(UseColor);
    }
}

void UCPlayerWidget::UpdateOverHealLerp()
{
    const float DeltaTime = 0.05f;  // 20hz
    
    // 목표값에 도달했는지 확인
    if (FMath::IsNearlyEqual(CurrentDisplayOverHealRatio, TargetOverHealRatio, 0.001f))
    {
        // 정확히 목표값으로 설정하고 Timer 중지
        CurrentDisplayOverHealRatio = TargetOverHealRatio;
        
        if (OverHealHpBar)
        {
            OverHealHpBar->SetPercent(CurrentDisplayOverHealRatio);
            
            if (CurrentDisplayOverHealRatio > 0.001f)
                OverHealHpBar->SetVisibility(ESlateVisibility::Visible);
            else
                OverHealHpBar->SetVisibility(ESlateVisibility::Hidden);
        }
        
        GetWorld()->GetTimerManager().ClearTimer(TimerHandle_OverHealLerp);
        return;
    }
    
    // Lerp 계산
    CurrentDisplayOverHealRatio = FMath::FInterpTo(CurrentDisplayOverHealRatio, TargetOverHealRatio, DeltaTime, OverHealLerpSpeed);
    
    // UI 업데이트
    if (OverHealHpBar)
    {
        OverHealHpBar->SetPercent(CurrentDisplayOverHealRatio);
        
        if (CurrentDisplayOverHealRatio > 0.001f)
            OverHealHpBar->SetVisibility(ESlateVisibility::Visible);
        else
            OverHealHpBar->SetVisibility(ESlateVisibility::Hidden);
    }
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
