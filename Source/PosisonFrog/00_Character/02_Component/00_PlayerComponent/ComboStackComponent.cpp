
#include "ComboStackComponent.h"
#include "GameFramework/Actor.h"

namespace
{
    constexpr float INVALID_TIME = -1.f;

    bool IsValidTime(float Time)
    {
        return Time >= 0.f;
    }
}

UComboStackComponent::UComboStackComponent()
{
    PrimaryComponentTick.bCanEverTick = true;

    auto MakeThreshold = [](EComboRank Rank, int32 Min, int32 Max) -> FComboRankThreshold
    {
        FComboRankThreshold Threshold;
        Threshold.Rank = Rank;
        Threshold.MinCSC = Min;
        Threshold.MaxCSC = Max;
        return Threshold;
    };
    
    RankThresholds = {
        MakeThreshold(EComboRank::D, 0, 5),
        MakeThreshold(EComboRank::C, 6, 11),
        MakeThreshold(EComboRank::B, 12, 17),
        MakeThreshold(EComboRank::A, 18, 23),
        MakeThreshold(EComboRank::S, 24, 30)
    };
}

void UComboStackComponent::BeginPlay()
{
    Super::BeginPlay();
    
    RefreshConfigFromTable();
    RefreshRanksFromTable();

    ResetInternal(true, false);
}

void UComboStackComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

    if (Config.bSameFrameGuard)
    {
        CleanupSameFrameRecords(Now);
    }

    if (UltState == EUltState::Casting)
    {
        // 궁극기 시전 중에는 감쇠를 진행하지 않음
        return;
    }

    HandleDecay(Now);
}

void UComboStackComponent::OnDirectHit(FName AttackId, float HitWorldTime)
{
    if (!GetWorld())
    {
        return;
    }

    const float Now = GetWorld()->GetTimeSeconds();
    const float EffectiveTime = HitWorldTime >= 0.f ? HitWorldTime : Now;

    if (Config.bSameFrameGuard)
    {
        const float* LastTime = LastAttackHitTimes.Find(AttackId);
        if (LastTime && FMath::Abs(*LastTime - EffectiveTime) <= Config.SameFrameTolerance)
        {
            return;
        }

        LastAttackHitTimes.Add(AttackId, EffectiveTime);
    }

    ExitDecay();

    const int32 NewCSC = FMath::Clamp(CSC + Config.GainPerHit, 0, Config.MaxStacks);
    SetCSC(NewCSC);

    LastHitTime = EffectiveTime;

    const EComboRank OldRank = Rank;
    const EComboRank NewRank = EvaluateRankForCSC(CSC);
    if (NewRank != OldRank)
    {
        SetRank(NewRank);
    }

    if (Rank == EComboRank::S && UltState != EUltState::Casting)
    {
        SetUltState(EUltState::Ready);

        if (!bHasPlayedFirstS)
        {
            bHasPlayedFirstS = true;
            OnFirstSRankAchieved.Broadcast();
        }
    }
    else if (Rank != EComboRank::S && UltState == EUltState::Ready)
    {
        SetUltState(EUltState::Normal);
    }
}

void UComboStackComponent::OnReset(ECSCResetReason Reason)
{
    ResetInternal(true, true);
}

bool UComboStackComponent::CanCastUlt() const
{
    return Rank == EComboRank::S && UltState == EUltState::Ready;
}

void UComboStackComponent::OnUltStarted(FName UltId)
{
    if (UltState == EUltState::Casting)
    {
        return;
    }

    ExitDecay();
    SetUltState(EUltState::Casting);
}

void UComboStackComponent::OnUltEnded(FName UltId)
{
    ResetInternal(true, true);
}

void UComboStackComponent::RefreshConfigFromTable()
{
    if (!ConfigTable)
    {
        return;
    }

    static const FString Context = TEXT("ComboStackConfig");
    TArray<FComboStackConfig*> Rows;
    ConfigTable->GetAllRows(Context, Rows);

    if (Rows.Num() > 0 && Rows[0])
    {
        Config = *Rows[0];
    }
}

void UComboStackComponent::RefreshRanksFromTable()
{
    if (!RankThresholdTable)
    {
        return;
    }

    static const FString Context = TEXT("ComboRankThreshold");
    TArray<FComboRankThreshold*> Rows;
    RankThresholdTable->GetAllRows(Context, Rows);

    if (Rows.Num() == 0)
    {
        return;
    }

    RankThresholds.Reset();
    for (FComboRankThreshold* Row : Rows)
    {
        if (Row)
        {
            RankThresholds.Add(*Row);
        }
    }

    RankThresholds.Sort([](const FComboRankThreshold& L, const FComboRankThreshold& R)
    {
        return static_cast<uint8>(L.Rank) < static_cast<uint8>(R.Rank);
    });
}

void UComboStackComponent::HandleDecay(float CurrentTime)
{
    if (!IsValidTime(LastHitTime))
    {
        return;
    }

   if (!bIsDecaying)
        {
       if (CurrentTime - LastHitTime >= Config.DecayStartDelay)
        {
            EnterDecay(CurrentTime);
        }
        else
        {
            return;
        }
    }

    if (bIsDecaying)
    {
        if (CurrentTime - LastDecayTime >= Config.DemoteInterval)
        {
            if (!TryDemoteOneStep(CurrentTime))
            {
                ExitDecay();
            }
            else
            {
                LastDecayTime = CurrentTime;
            }
        }
    }
}

void UComboStackComponent::EnterDecay(float CurrentTime)
{
    bIsDecaying = true;
    LastDecayTime = CurrentTime;
}

void UComboStackComponent::ExitDecay()
{
    bIsDecaying = false;
    LastDecayTime = INVALID_TIME;
}

bool UComboStackComponent::TryDemoteOneStep(float CurrentTime)
{
    const int32 CurrentIndex = GetRankIndex(Rank);
    const int32 FloorIndex = GetRankIndex(Config.DemoteFloorRank);

    if (CurrentIndex == INDEX_NONE)
    {
        return false;
    }

    const int32 EffectiveFloorIndex = FloorIndex != INDEX_NONE ? FloorIndex : 0;

    if (CurrentIndex <= EffectiveFloorIndex)
    {
        return false;
    }

    const int32 TargetIndex = CurrentIndex - 1;
    if (!RankThresholds.IsValidIndex(TargetIndex))
    {
        return false;
    }

    const EComboRank NewRank = RankThresholds[TargetIndex].Rank;
    SetRank(NewRank, true);
    SetCSC(GetMaxCSCForRank(NewRank));

    if (NewRank != EComboRank::S && UltState == EUltState::Ready)
    {
        SetUltState(EUltState::Normal);
    }

    if (NewRank == Config.DemoteFloorRank)
    {
        return false;
    }

    return true;
}

void UComboStackComponent::SetCSC(int32 NewValue, bool bBroadcast, bool bForceBroadcast)
{
    NewValue = FMath::Clamp(NewValue, 0, Config.MaxStacks);
    const bool bChanged = CSC != NewValue;
    if (!bChanged && !bForceBroadcast)
    {
        return;
    }

    CSC = NewValue;

    if (bBroadcast)
    {
        OnCSCChanged.Broadcast(CSC);
    }
}

void UComboStackComponent::SetRank(EComboRank NewRank, bool bForceBroadcast)
{
    if (!bForceBroadcast && Rank == NewRank)
    {
        return;
    }

    const EComboRank OldRank = Rank;
    Rank = NewRank;

    OnRankChanged.Broadcast(OldRank, Rank);

    if (Rank == EComboRank::S && UltState != EUltState::Casting)
    {
        SetUltState(EUltState::Ready);
        if (!bHasPlayedFirstS)
        {
            bHasPlayedFirstS = true;
            OnFirstSRankAchieved.Broadcast();
        }
    }
    else if (Rank != EComboRank::S && UltState == EUltState::Ready)
    {
        SetUltState(EUltState::Normal);
    }
}

void UComboStackComponent::SetUltState(EUltState NewState)
{
    if (UltState == NewState)
    {
        return;
    }

    UltState = NewState;
    OnUltStateChanged.Broadcast(UltState);
}

EComboRank UComboStackComponent::EvaluateRankForCSC(int32 Value) const
{
    for (const FComboRankThreshold& Threshold : RankThresholds)
    {
        if (Value >= Threshold.MinCSC && Value <= Threshold.MaxCSC)
        {
            return Threshold.Rank;
        }
    }

    return RankThresholds.Num() > 0 ? RankThresholds[0].Rank : EComboRank::D;
}

int32 UComboStackComponent::GetMaxCSCForRank(EComboRank InRank) const
{
    for (const FComboRankThreshold& Threshold : RankThresholds)
    {
        if (Threshold.Rank == InRank)
        {
            return Threshold.MaxCSC;
        }
    }
    return Config.MaxStacks;
}

int32 UComboStackComponent::GetRankIndex(EComboRank InRank) const
{
    for (int32 Index = 0; Index < RankThresholds.Num(); ++Index)
    {
        if (RankThresholds[Index].Rank == InRank)
        {
            return Index;
        }
    }

    return INDEX_NONE;
}

void UComboStackComponent::ResetTimers()
{
    LastHitTime = INVALID_TIME;
    LastDecayTime = INVALID_TIME;
    bIsDecaying = false;
}

void UComboStackComponent::ResetInternal(bool bResetCSC, bool bBroadcast)
{
    if (bResetCSC)
    {
        SetCSC(0, bBroadcast, true);
    }

    SetRank(EComboRank::D, true);
    if (UltState != EUltState::Normal)
    {
        SetUltState(EUltState::Normal);
    }

    ResetTimers();

    if (Config.bSameFrameGuard)
    {
        LastAttackHitTimes.Reset();
    }
}

void UComboStackComponent::CleanupSameFrameRecords(float CurrentTime)
{
    if (!Config.bSameFrameGuard)
    {
        return;
    }

    for (auto It = LastAttackHitTimes.CreateIterator(); It; ++It)
    {
        if (CurrentTime - It.Value() > Config.SameFrameWindow)
        {
            It.RemoveCurrent();
        }
    }
}

