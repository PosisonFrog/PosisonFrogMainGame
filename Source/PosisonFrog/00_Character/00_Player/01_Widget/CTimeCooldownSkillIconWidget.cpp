// CTimeCooldownSkillIconWidget.cpp

#include "00_Character/00_Player/01_Widget/CTimeCooldownSkillIconWidget.h"

#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"

void UCTimeCooldownSkillIconWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 초기화
    bIsCoolDown = false;
    MaxCooldownSeconds = 0.f;
    LastPercent = 0.f;

    if (SkillBar)
    {
        SkillBar->SetPercent(0.f);
    }

    if (SkillIcon)
    {
        SkillIcon->SetColorAndOpacity(IconTint_Normal);
    }
}

void UCTimeCooldownSkillIconWidget::StartCooldown(float InMaxSeconds)
{
    MaxCooldownSeconds = FMath::Max(0.f, InMaxSeconds);
    bIsCoolDown = MaxCooldownSeconds > 0.f;

    // 시작 연출
    if (Anim_CooldownStart)
        PlayAnimation(Anim_CooldownStart);

    if (SkillIcon)
        SkillIcon->SetColorAndOpacity(IconTint_OnCooldown);

    // 시작 시점은 100% 차있는 상태(남은시간=최대시간)로 보이도록
    RefreshUI(MaxCooldownSeconds > 0.f ? 1.f : 0.f);
}

void UCTimeCooldownSkillIconWidget::UpdateCooldownByRemaining(float RemainingSeconds)
{
    if (!SkillBar || MaxCooldownSeconds <= 0.f)
        return;

    const float Pct = Normalize01(RemainingSeconds, MaxCooldownSeconds);
    RefreshUI(Pct);

    // 진행 중 짤막한 틱 애니가 있으면 여기서
    if (Anim_CooldownTick && bIsCoolDown)
        PlayAnimationForward(Anim_CooldownTick);
}

void UCTimeCooldownSkillIconWidget::UpdateCooldownByElapsed(float ElapsedSeconds)
{
    if (MaxCooldownSeconds <= 0.f)
        return;

    const float Remaining = FMath::Max(0.f, MaxCooldownSeconds - FMath::Max(0.f, ElapsedSeconds));
    UpdateCooldownByRemaining(Remaining);
}

void UCTimeCooldownSkillIconWidget::FinishCooldown(bool bPlayEffects /*=true*/)
{
    // 쿨다운 종료
    bIsCoolDown = false;
    MaxCooldownSeconds = 0.f;

    // 바/아이콘 원복
    RefreshUI(0.f);
    if (SkillIcon)
        SkillIcon->SetColorAndOpacity(IconTint_Normal);

    // 종료 연출
    if (Anim_CooldownFinish)
        PlayAnimation(Anim_CooldownFinish);

    if (bPlayEffects)
    {
        PlaySfx(SFX_CooldownFinished);
        PlayFinishEffectsOnPlayer();
    }
}

void UCTimeCooldownSkillIconWidget::NotifyCooldownBlocked()
{
    PlaySfx(SFX_CooldownBlocked);
}

// ───────────────────────── 헬퍼/내부 ─────────────────────────

float UCTimeCooldownSkillIconWidget::Normalize01(float Value, float Max)
{
    if (Max <= 0.f) return 0.f;
    const float P = Value / Max;          // 0~1로 정규화
    return FMath::Clamp(P, 0.f, 1.f);
}

void UCTimeCooldownSkillIconWidget::RefreshUI(float NormalizedPercent)
{
    // 변경 없으면 스킵(불필요한 Slate 갱신/재틴트 방지)
    if (FMath::IsNearlyEqual(NormalizedPercent, LastPercent, KINDA_SMALL_NUMBER))
        return;

    LastPercent = NormalizedPercent;

    if (SkillBar)
        SkillBar->SetPercent(NormalizedPercent);

    // 아이콘 틴트 전환(쿨다운 시작 시 1→(0,1), 종료 시 (0,1)→0)
    if (SkillIcon)
    {
        if (bIsCoolDown && !SkillIcon->ColorAndOpacity.Equals(IconTint_OnCooldown))
        {
            SkillIcon->SetColorAndOpacity(IconTint_OnCooldown);
        }
        else if (!bIsCoolDown && !SkillIcon->ColorAndOpacity.Equals(IconTint_Normal))
        {
            SkillIcon->SetColorAndOpacity(IconTint_Normal);
        }
    }
}

void UCTimeCooldownSkillIconWidget::PlaySfx(USoundBase* SFX)
{
    if (!SFX) return;
    if (APlayerController* PC = GetOwningPlayer())
    {
        UGameplayStatics::PlaySound2D(PC, SFX);
    }
    else
    {
        UGameplayStatics::PlaySound2D(this, SFX);
    }
}

void UCTimeCooldownSkillIconWidget::PlayFinishEffectsOnPlayer()
{
    if (!VFX_CooldownFinishedOnPlayer) return;

    if (APlayerController* PC = GetOwningPlayer())
    {
        if (APawn* Pawn = PC->GetPawn())
        {
            UNiagaraFunctionLibrary::SpawnSystemAtLocation(
                GetWorld(),
                VFX_CooldownFinishedOnPlayer,
                Pawn->GetActorLocation(),
                Pawn->GetActorRotation());
        }
    }
}

