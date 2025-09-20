// CSkillIconBaseWidget.cpp
#include "00_Character/00_Player/01_Widget/CSkillIconBaseWidget.h"

#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

void UCSkillIconBaseWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 초기화
    bInCooldown        = false;
    CooldownTotal      = 0.f;
    CooldownRemaining  = 0.f;
    CooldownEndTime    = 0.f;

    // 시작 시 진행바 초기 상태
    if (SkillBar)
    {
        SkillBar->SetPercent(ReadyFillValue); // 기본적으로 '준비됨' 상태를 표현
    }
}

void UCSkillIconBaseWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!bInCooldown)
        return;

    UWorld* World = GetWorld();
    if (!World)
        return;

    const float Now = World->GetTimeSeconds();
    CooldownRemaining = FMath::Max(0.f, CooldownEndTime - Now);

    if (CooldownTotal <= KINDA_SMALL_NUMBER)
    {
        // 예외 방지: 총 시간이 0에 가까우면 즉시 종료로 본다
        FinishCooldown_Internal();
        return;
    }

    const float NormalizedRemaining = FMath::Clamp(CooldownRemaining / CooldownTotal, 0.f, 1.f);
    const float DisplayValue = bInverseFill ? NormalizedRemaining : (1.f - NormalizedRemaining);
    UpdateProgressBar(DisplayValue);

    if (CooldownRemaining <= 0.f)
    {
        FinishCooldown_Internal();
    }
}

bool UCSkillIconBaseWidget::TryStartCooldown(float InTotalSeconds)
{
    if (bInCooldown)
    {
        // 이미 쿨타임: 차단 연출
        PlayCoolTimeBlockedSound();
        PlayCoolTimeBlockAnim();
        PlayCoolTimeBlockedEffect();
        return false;
    }

    StartCooldown(InTotalSeconds);
    return true;
}

void UCSkillIconBaseWidget::StartCooldown(float InTotalSeconds)
{
    UWorld* World = GetWorld();
    if (!World)
        return;

    CooldownTotal     = FMath::Max(0.f, InTotalSeconds);
    CooldownRemaining = CooldownTotal;
    CooldownEndTime   = World->GetTimeSeconds() + CooldownTotal;
    bInCooldown       = (CooldownTotal > KINDA_SMALL_NUMBER);

    // 쿨타임 시작 시 진행바를 '최대'로 설정
    if (SkillBar)
    {
        const float StartValue = bInverseFill ? 1.0f : 0.0f;
        SkillBar->SetPercent(StartValue);
    }
}

void UCSkillIconBaseWidget::NotifySkillPressed()
{
    if (bInCooldown)
    {
        PlayCoolTimeBlockedSound();
        PlayCoolTimeBlockAnim();
        PlayCoolTimeBlockedEffect();
    }
    // 쿨타임이 아니면 여기서 별도 처리 없음(실제 스킬 발동은 게임 로직에서)
}

void UCSkillIconBaseWidget::SetIcon(UTexture2D* InTex)
{
    if (SkillIcon && InTex)
    {
        SkillIcon->SetBrushFromTexture(InTex, true);
    }
}

void UCSkillIconBaseWidget::UpdateProgressBar(float Normalized)
{
    if (SkillBar)
    {
        SkillBar->SetPercent(FMath::Clamp(Normalized, 0.f, 1.f));
    }
}

void UCSkillIconBaseWidget::FinishCooldown_Internal()
{
    bInCooldown        = false;
    CooldownRemaining  = 0.f;
    CooldownTotal      = FMath::Max(0.f, CooldownTotal); // 그대로 유지
    CooldownEndTime    = 0.f;

    // 진행바를 '준비됨' 상태 값으로 맞춤
    if (SkillBar)
    {
        SkillBar->SetPercent(ReadyFillValue); // 역채움이면 0.0, 정채움이면 1.0 권장
    }

    PlaySkillReadySound();
    PlaySkillReadyAnim();
    PlaySkillReadyEffect();

    OnCooldownFinished.Broadcast();
}

// ─── 연출 유틸 ───

void UCSkillIconBaseWidget::PlaySkillReadySound()
{
    if (SFX_SkillReady)
    {
        UGameplayStatics::PlaySound2D(this, SFX_SkillReady);
    }
}

void UCSkillIconBaseWidget::PlayCoolTimeBlockedSound()
{
    if (SFX_CoolTimeBlocked)
    {
        UGameplayStatics::PlaySound2D(this, SFX_CoolTimeBlocked);
    }
}

void UCSkillIconBaseWidget::PlaySkillReadyAnim()
{
    if (Anim_SkillReady)
    {
        PlayAnimation(Anim_SkillReady, 0.f, 1, EUMGSequencePlayMode::Forward, 1.f);
    }
}

void UCSkillIconBaseWidget::PlayCoolTimeBlockAnim()
{
    if (Anim_CoolBlocked)
    {
        PlayAnimation(Anim_CoolBlocked, 0.f, 1, EUMGSequencePlayMode::Forward, 1.f);
    }
}

void UCSkillIconBaseWidget::PlaySkillReadyEffect()
{
    if (!VFX_CoolTimeFinishedOnPlayer) return;

    if (APlayerController* PC = GetOwningPlayer())
    {
        if (APawn* Pawn = PC->GetPawn())
        {
            UNiagaraFunctionLibrary::SpawnSystemAtLocation(
                GetWorld(),
                VFX_CoolTimeFinishedOnPlayer,
                Pawn->GetActorLocation(),
                Pawn->GetActorRotation()
            );
        }
    }
}

void UCSkillIconBaseWidget::PlayCoolTimeBlockedEffect()
{
    if (!VFX_CoolBlocked) return;

    if (APlayerController* PC = GetOwningPlayer())
    {
        if (APawn* Pawn = PC->GetPawn())
        {
            UNiagaraFunctionLibrary::SpawnSystemAtLocation(
                GetWorld(),
                VFX_CoolBlocked,
                Pawn->GetActorLocation(),
                Pawn->GetActorRotation()
            );
        }
    }
}