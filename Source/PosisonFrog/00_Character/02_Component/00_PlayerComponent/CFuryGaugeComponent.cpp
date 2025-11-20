// Fill out your copyright notice in the Description page of Project Settings.


#include "00_Character/02_Component/00_PlayerComponent/CFuryGaugeComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"

UCFuryGaugeComponent::UCFuryGaugeComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    if (Tiers.Num() == 0)
    {
        Tiers.Add({1, 3, 1.0f, 100.f});
        Tiers.Add({4, 6, 1.5f, 150.f});
        Tiers.Add({7, 9, 2.0f, 200.f});
        Tiers.Add({10,10,2.5f, 250.f});
    }
}

void UCFuryGaugeComponent::BeginPlay()
{
    Super::BeginPlay();
}

void UCFuryGaugeComponent::AddStack(int32 Amount)
{
    if (Amount <= 0) return;
    if (bEffectActive && bBlockStackWhileActive) return;

    const int32 NewStacks = FMath::Clamp(CurrentStacks + Amount, 0, MaxStacks);
    if (NewStacks != CurrentStacks)
    {
        CurrentStacks = NewStacks;
        OnStacksChanged.Broadcast(CurrentStacks, MaxStacks);
    }
}

void UCFuryGaugeComponent::SetFury(int32 NewStacks)
{
    CurrentStacks = FMath::Clamp(NewStacks, 0, MaxStacks);
    OnStacksChanged.Broadcast(CurrentStacks, MaxStacks);
}

int32 UCFuryGaugeComponent::FindTierIndexForStacks(int32 Stacks) const
{
    for (int32 i=0; i<Tiers.Num(); ++i)
        if (Stacks >= Tiers[i].MinStacks && Stacks <= Tiers[i].MaxStacks)
            return i;
    return -1;
}

bool UCFuryGaugeComponent::ActivateEffect()
{
    if (bEffectActive)  return false;
    if (CurrentStacks <= 0) return false;

    UWorld* World = GetWorld();
    if (!World) return false;
    
    InitialStacksAtActivation = CurrentStacks;
    bFinisherTriggered = false;

    const int32 TierIdx = FindTierIndexForStacks(InitialStacksAtActivation);
    if (TierIdx < 0) return false;

    const FFuryGaugeTier& Tier = Tiers[TierIdx];

    bEffectActive       = true;
    ActiveTierIndex     = TierIdx;
    ActiveTotalDuration = Tier.Duration;
    ActiveTotalDamage   = Tier.TotalDamage;

    const float Now = World->GetTimeSeconds();
    ActiveEndTime = Now + ActiveTotalDuration;

    // 발동과 동시에 전량 소모
    CurrentStacks = 0;
    OnStacksChanged.Broadcast(CurrentStacks, MaxStacks);

    // 자동 종료 타이머
    FTimerManager& TimerManager = World->GetTimerManager();
        TimerManager.ClearTimer(TimerHandle_EffectEnd);
        TimerManager.SetTimer(
        TimerHandle_EffectEnd,
        [this]() { EndEffectInternal(false, 0.f); },  // 자연 종료
        ActiveTotalDuration, false
    );

    // UI 틱
    if (EffectUITickInterval > 0.f)
    {
        TimerManager.ClearTimer(TimerHandle_EffectUITick);
        TimerManager.SetTimer(
            TimerHandle_EffectUITick,
            this, &UCFuryGaugeComponent::EffectUITick,
            EffectUITickInterval, true
        );
        EffectUITick();
    }

    OnEffectStarted.Broadcast(ActiveTierIndex, ActiveTotalDuration, ActiveTotalDamage, InitialStacksAtActivation);
    return true;
}

bool UCFuryGaugeComponent::CancelEffect()
{
    if (!bEffectActive) return false;

    const float Remain = GetEffectTimeRemaining();
    GetWorld()->GetTimerManager().ClearTimer(TimerHandle_EffectEnd);
    GetWorld()->GetTimerManager().ClearTimer(TimerHandle_EffectUITick);
    if (UWorld* World = GetWorld())
    {
        FTimerManager& TimerManager = World->GetTimerManager();
        TimerManager.ClearTimer(TimerHandle_EffectEnd);
        TimerManager.ClearTimer(TimerHandle_EffectUITick);
    }
    EndEffectInternal(true, Remain); // 취소 종료(10칸이면 즉시 피니시)
    OnEffectEnded.Broadcast(true, FMath::Max(0.f, Remain));
    return true;
}

void UCFuryGaugeComponent::EndEffectInternal(bool bCanceled, float /*CanceledRemainTime*/)
{
    if (!bEffectActive) return;

    const bool bShouldFinisher =
        !bCanceled &&
        (InitialStacksAtActivation >= MaxStacks) && !bFinisherTriggered;

    if (bShouldFinisher)
    {
        bFinisherTriggered = true;
        OnFinisherTriggered.Broadcast(FinisherDamageAtMaxStacks); // ‘망치 내려찍기’ 1타
    }

    bEffectActive       = false;
    ActiveEndTime       = -1.f;
    ActiveTotalDuration = 0.f;
    ActiveTotalDamage   = 0.f;
    ActiveTierIndex     = -1;

    if (TimerHandle_EffectEnd.IsValid())
    {
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(TimerHandle_EffectEnd);
        }
    }
    if (TimerHandle_EffectUITick.IsValid())
    {
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(TimerHandle_EffectUITick);
        }
    }

    if (!bCanceled)
    {
        OnEffectEnded.Broadcast(false, 0.f);
    }

    InitialStacksAtActivation = 0; // 다음 사이클은 항상 0칸부터
}

float UCFuryGaugeComponent::GetEffectTimeRemaining() const
{
    if (!bEffectActive) return 0.f;
    const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    return FMath::Max(0.f, ActiveEndTime - Now);
}

void UCFuryGaugeComponent::EffectUITick()
{
    if (!bEffectActive) return;
    OnEffectTick.Broadcast(GetEffectTimeRemaining(), ActiveTotalDuration);
}
