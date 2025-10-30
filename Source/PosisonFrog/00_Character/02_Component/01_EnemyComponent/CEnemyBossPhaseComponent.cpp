#include "00_Character/02_Component/01_EnemyComponent/CEnemyBossPhaseComponent.h"

#include "00_Character/02_Component/01_EnemyComponent/CEnemyHealthComponent.h"
#include "GameFramework/Actor.h"

namespace
{
static const FBossPhaseDefinition GDummyPhase;
}

UCEnemyBossPhaseComponent::UCEnemyBossPhaseComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UCEnemyBossPhaseComponent::BeginPlay()
{
    Super::BeginPlay();

    InitializePhases();

    if (AActor* Owner = GetOwner())
    {
        CachedHealth = Owner->FindComponentByClass<UCEnemyHealthComponent>();
        if (CachedHealth.IsValid())
        {
            CachedHealth->OnHealthChanged.AddDynamic(this, &UCEnemyBossPhaseComponent::HandleHealthChanged);
            CachedHealth->OnDeath.AddDynamic(this, &UCEnemyBossPhaseComponent::HandleDeath);

            const float MaxHp = CachedHealth->GetMaxHealth();
            const float CurHp = CachedHealth->GetHealth();
            if (MaxHp > KINDA_SMALL_NUMBER)
            {
                EvaluatePhaseByHealth(CurHp / MaxHp);
            }
        }
    }

    if (PhaseData)
    {
        ShoutTriggerPower = PhaseData->MaxPower * PhaseData->ShoutTriggerRatio;
        CurrentPower = FMath::Clamp(CurrentPower, 0.f, PhaseData->MaxPower);
    }
}

void UCEnemyBossPhaseComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (CachedHealth.IsValid())
    {
        CachedHealth->OnHealthChanged.RemoveDynamic(this, &UCEnemyBossPhaseComponent::HandleHealthChanged);
        CachedHealth->OnDeath.RemoveDynamic(this, &UCEnemyBossPhaseComponent::HandleDeath);
    }

    Super::EndPlay(EndPlayReason);
}

void UCEnemyBossPhaseComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!PhaseData || State == EBossBattleState::Dead)
    {
        return;
    }

    UpdateCooldowns(DeltaTime);

    if (!bBattleStarted)
    {
        return;
    }

    UpdatePassivePower(DeltaTime);
    TryTriggerShout();

    if (State == EBossBattleState::Dormant || State == EBossBattleState::Dead)
    {
        return;
    }

    TickState(DeltaTime);
}

void UCEnemyBossPhaseComponent::StartBattle(bool bSkipIntro)
{
    if (bBattleStarted)
    {
        return;
    }

    bBattleStarted = true;
    if (!PhaseData)
    {
        EnterState(EBossBattleState::Intro, 0.f);
        AdvanceState();
        return;
    }

    if (bSkipIntro)
    {
        EnterState(EBossBattleState::Recover, GetCurrentPhase().BaseRecoveryDuration);
    }
    else
    {
        EnterState(EBossBattleState::Intro, GetCurrentPhase().IntroDuration);
    }
}

void UCEnemyBossPhaseComponent::ForceNextPattern(FName PatternId)
{
    if (PatternId.IsNone() || !PhaseData)
    {
        return;
    }

    ForcedPatternId = PatternId;
}

void UCEnemyBossPhaseComponent::AddPower(float Amount)
{
    if (Amount <= 0.f)
    {
        return;
    }

    ApplyPowerDelta(Amount);
}

void UCEnemyBossPhaseComponent::ConsumePower(float Amount)
{
    if (Amount <= 0.f)
    {
        return;
    }

    ApplyPowerDelta(-Amount);
}

const FBossPhaseDefinition& UCEnemyBossPhaseComponent::GetCurrentPhase() const
{
    if (!PhaseData || !PhaseData->Phases.IsValidIndex(CurrentPhaseIndex))
    {
        return GDummyPhase;
    }
    return PhaseData->Phases[CurrentPhaseIndex];
}

void UCEnemyBossPhaseComponent::InitializePhases()
{
    PhaseOrder.Reset();
    if (!PhaseData)
    {
        return;
    }

    for (int32 Idx = 0; Idx < PhaseData->Phases.Num(); ++Idx)
    {
        PhaseOrder.Add(Idx);
    }

    PhaseOrder.Sort([this](int32 L, int32 R)
    {
        const float LH = PhaseData->Phases[L].EnterHealthRatio;
        const float RH = PhaseData->Phases[R].EnterHealthRatio;
        return LH > RH;
    });

    if (PhaseData->Phases.Num() > 0)
    {
        CurrentPhaseIndex = PhaseOrder[0];
        ResetPhaseRuntime();
        OnPhaseChanged.Broadcast(CurrentPhaseIndex, GetCurrentPhase());
    }
}

void UCEnemyBossPhaseComponent::EvaluatePhaseByHealth(float HealthRatio)
{
    if (!PhaseData || PhaseOrder.Num() == 0)
    {
        return;
    }

    int32 DesiredPhase = PhaseOrder[0];
    for (int32 Index : PhaseOrder)
    {
        const FBossPhaseDefinition& Phase = PhaseData->Phases[Index];
        if (HealthRatio <= Phase.EnterHealthRatio + KINDA_SMALL_NUMBER)
        {
            DesiredPhase = Index;
        }
    }

    if (DesiredPhase != CurrentPhaseIndex)
    {
        EnterPhase(DesiredPhase);
    }
}

void UCEnemyBossPhaseComponent::EnterPhase(int32 PhaseIndex)
{
    if (!PhaseData || !PhaseData->Phases.IsValidIndex(PhaseIndex))
    {
        return;
    }

    CurrentPhaseIndex = PhaseIndex;
    ResetPhaseRuntime();

    OnPhaseChanged.Broadcast(CurrentPhaseIndex, GetCurrentPhase());

    if (bBattleStarted)
    {
        EnterState(EBossBattleState::PhaseIntro, GetCurrentPhase().IntroDuration);
    }
}

void UCEnemyBossPhaseComponent::ResetPhaseRuntime()
{
    CurrentPatternIndex = INDEX_NONE;
    PatternCooldowns.SetNumZeroed(GetCurrentPhase().Patterns.Num());
}

void UCEnemyBossPhaseComponent::EnterState(EBossBattleState NewState, float Duration)
{
    if (State == NewState)
    {
        StateTimeRemaining = Duration;
        return;
    }

    const EBossBattleState Prev = State;
    State = NewState;
    StateTimeRemaining = Duration;

    OnStateChanged.Broadcast(State, Prev);

    if (State == EBossBattleState::ExecutingPattern && CurrentPatternIndex != INDEX_NONE)
    {
        const FBossPatternDefinition& Pattern = GetCurrentPhase().Patterns[CurrentPatternIndex];
        OnPatternStarted.Broadcast(CurrentPhaseIndex, Pattern.PatternId, Pattern, CurrentPower);
    }
    else if (State == EBossBattleState::Shout)
    {
        OnShoutStarted.Broadcast(CurrentPhaseIndex, GetCurrentPhase().ShoutMetaId, GetCurrentPhase().ShoutDuration);
    }
}

void UCEnemyBossPhaseComponent::TickState(float DeltaTime)
{
    if (StateTimeRemaining > 0.f)
    {
        StateTimeRemaining = FMath::Max(0.f, StateTimeRemaining - DeltaTime);
    }

    if (StateTimeRemaining <= 0.f)
    {
        AdvanceState();
    }
}

void UCEnemyBossPhaseComponent::AdvanceState()
{
    switch (State)
    {
    case EBossBattleState::Intro:
        EnterState(EBossBattleState::PhaseIntro, GetCurrentPhase().IntroDuration);
        break;
    case EBossBattleState::PhaseIntro:
        EnterState(EBossBattleState::Recover, GetCurrentPhase().BaseRecoveryDuration);
        break;
    case EBossBattleState::Recover:
        {
            const int32 NextPattern = SelectNextPatternIndex();
            if (NextPattern != INDEX_NONE)
            {
                BeginPattern(NextPattern);
            }
            else
            {
                EnterState(EBossBattleState::Recover, 0.5f);
            }
        }
        break;
    case EBossBattleState::ExecutingPattern:
        FinishPattern(false);
        break;
    case EBossBattleState::Shout:
        EnterState(EBossBattleState::Recover, GetCurrentPhase().BaseRecoveryDuration);
        OnShoutFinished.Broadcast(CurrentPhaseIndex, GetCurrentPhase().ShoutMetaId, GetCurrentPhase().ShoutDuration);
        CurrentPower = FMath::Min(CurrentPower, ShoutTriggerPower * 0.25f);
        break;
    default:
        break;
    }
}

void UCEnemyBossPhaseComponent::UpdateCooldowns(float DeltaTime)
{
    for (float& Cooldown : PatternCooldowns)
    {
        Cooldown = FMath::Max(0.f, Cooldown - DeltaTime);
    }

}

void UCEnemyBossPhaseComponent::UpdatePassivePower(float DeltaTime)
{
    if (!PhaseData)
    {
        return;
    }

    const float GainRate = PhaseData->PassivePowerPerSecond * GetCurrentPhase().PowerGainMultiplier;
    if (GainRate > 0.f)
    {
        ApplyPowerDelta(GainRate * DeltaTime);
    }
}

void UCEnemyBossPhaseComponent::TryTriggerShout()
{
    if (!PhaseData || !bBattleStarted)
    {
        return;
    }

    if (GetCurrentPhase().ShoutMetaId.IsNone())
    {
        return;
    }

    if (State == EBossBattleState::Shout || State == EBossBattleState::Dead)
    {
        return;
    }

    if (CurrentPower >= ShoutTriggerPower && ShoutTriggerPower > 0.f)
    {
        EnterState(EBossBattleState::Shout, GetCurrentPhase().ShoutDuration);
        CurrentPower = PhaseData->MaxPower;
    }
}

void UCEnemyBossPhaseComponent::ApplyPowerDelta(float Delta)
{
    if (!PhaseData)
    {
        return;
    }

    const float OldPower = CurrentPower;
    CurrentPower = FMath::Clamp(CurrentPower + Delta, 0.f, PhaseData->MaxPower);

    if (!FMath::IsNearlyEqual(OldPower, CurrentPower))
    {
        // 파워 변경 시에도 샤우트 조건을 재검증한다.
        TryTriggerShout();
    }
}

void UCEnemyBossPhaseComponent::BeginPattern(int32 PatternIndex)
{
    if (!PhaseData)
    {
        return;
    }

    const TArray<FBossPatternDefinition>& Patterns = GetCurrentPhase().Patterns;
    if (!Patterns.IsValidIndex(PatternIndex))
    {
        return;
    }

    CurrentPatternIndex = PatternIndex;
    const FBossPatternDefinition& Pattern = Patterns[PatternIndex];

    if (PatternCooldowns.IsValidIndex(PatternIndex))
    {
        PatternCooldowns[PatternIndex] = Pattern.Cooldown;
    }

    const float Drain = (Pattern.PowerCost + Pattern.RequiredPower) * GetCurrentPhase().PowerDrainMultiplier;
    if (Drain > 0.f)
    {
        ApplyPowerDelta(-Drain);
    }

    ForcedPatternId = NAME_None;

    EnterState(EBossBattleState::ExecutingPattern, Pattern.ExecutionTime);
}

void UCEnemyBossPhaseComponent::FinishPattern(bool bInterrupted)
{
    if (!PhaseData)
    {
        return;
    }

    if (CurrentPatternIndex == INDEX_NONE)
    {
        EnterState(EBossBattleState::Recover, GetCurrentPhase().BaseRecoveryDuration);
        return;
    }

    const FBossPatternDefinition& Pattern = GetCurrentPhase().Patterns[CurrentPatternIndex];
    if (!bInterrupted)
    {
        ApplyPowerDelta(Pattern.PowerReward);
    }
    else if (PhaseData->PowerLossOnInterrupt > 0.f)
    {
        ApplyPowerDelta(-PhaseData->PowerLossOnInterrupt);
    }

    OnPatternFinished.Broadcast(CurrentPhaseIndex, Pattern.PatternId, Pattern, CurrentPower);

    CurrentPatternIndex = INDEX_NONE;
    EnterState(EBossBattleState::Recover, FMath::Max(GetCurrentPhase().BaseRecoveryDuration, Pattern.RecoveryTime));
}

int32 UCEnemyBossPhaseComponent::SelectNextPatternIndex() const
{
    if (!PhaseData)
    {
        return INDEX_NONE;
    }

    const TArray<FBossPatternDefinition>& Patterns = GetCurrentPhase().Patterns;
    if (Patterns.Num() == 0)
    {
        return INDEX_NONE;
    }

    if (!ForcedPatternId.IsNone())
    {
        for (int32 Index = 0; Index < Patterns.Num(); ++Index)
        {
            if (Patterns[Index].PatternId == ForcedPatternId && CanUsePattern(Index))
            {
                return Index;
            }
        }
    }

    float TotalWeight = 0.f;
    TArray<float> AccWeights;
    AccWeights.Reserve(Patterns.Num());

    for (int32 Index = 0; Index < Patterns.Num(); ++Index)
    {
        if (!CanUsePattern(Index))
        {
            AccWeights.Add(TotalWeight);
            continue;
        }

        TotalWeight += FMath::Max(0.f, Patterns[Index].Weight);
        AccWeights.Add(TotalWeight);
    }

    if (TotalWeight <= 0.f)
    {
        return INDEX_NONE;
    }

    const float Sample = FMath::FRandRange(0.f, TotalWeight);
    for (int32 Index = 0; Index < AccWeights.Num(); ++Index)
    {
        if (Sample <= AccWeights[Index])
        {
            return Index;
        }
    }

    return Patterns.Num() - 1;
}

bool UCEnemyBossPhaseComponent::CanUsePattern(int32 PatternIndex) const
{
    if (!PhaseData)
    {
        return false;
    }

    const TArray<FBossPatternDefinition>& Patterns = GetCurrentPhase().Patterns;
    if (!Patterns.IsValidIndex(PatternIndex))
    {
        return false;
    }

    if (PatternCooldowns.IsValidIndex(PatternIndex) && PatternCooldowns[PatternIndex] > 0.f)
    {
        return false;
    }

    const FBossPatternDefinition& Pattern = Patterns[PatternIndex];
    if (CurrentPower < Pattern.RequiredPower)
    {
        return false;
    }

    return true;
}

void UCEnemyBossPhaseComponent::HandleHealthChanged(float Current, float Max)
{
    if (Max <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    EvaluatePhaseByHealth(Current / Max);
}

void UCEnemyBossPhaseComponent::HandleDeath(AActor* DeadActor)
{
    if (State == EBossBattleState::ExecutingPattern && PhaseData)
    {
        if (GetCurrentPhase().Patterns.IsValidIndex(CurrentPatternIndex))
        {
            const FBossPatternDefinition Pattern = GetCurrentPhase().Patterns[CurrentPatternIndex];
            if (PhaseData->PowerLossOnInterrupt > 0.f)
            {
                ApplyPowerDelta(-PhaseData->PowerLossOnInterrupt);
            }
            OnPatternFinished.Broadcast(CurrentPhaseIndex, Pattern.PatternId, Pattern, CurrentPower);
        }
        CurrentPatternIndex = INDEX_NONE;
    }

    EnterState(EBossBattleState::Dead, 0.f);
    bBattleStarted = false;
}
