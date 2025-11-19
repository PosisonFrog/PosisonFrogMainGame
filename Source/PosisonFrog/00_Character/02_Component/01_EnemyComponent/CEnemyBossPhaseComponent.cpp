#include "00_Character/02_Component/01_EnemyComponent/CEnemyBossPhaseComponent.h"

#include "00_Character/02_Component/01_EnemyComponent/CEnemyHealthComponent.h"
#include "GameFramework/Actor.h"
#include "00_Character/01_Enemy/01_AIController/BossAIController.h"

namespace
{
static const FBossPhaseDefinition GDummyPhase;
}

UCEnemyBossPhaseComponent::UCEnemyBossPhaseComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
    PrimaryComponentTick.bCanEverTick = true;
    CurrentPower = 0.f;
    CurrentPhaseIndex = INDEX_NONE;
    CombatState = EBossCombatState::None;
    StateTimeRemaining = 0.f;
    PendingRecoveryDuration = 0.f;
}

void UCEnemyBossPhaseComponent::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Error, TEXT("[BossPhaseComponent] ========================================"));
    UE_LOG(LogTemp, Error, TEXT("[BossPhaseComponent] BeginPlay - bBattleStarted = %s"), bBattleStarted ? TEXT("TRUE") : TEXT("FALSE"));
    UE_LOG(LogTemp, Error, TEXT("[BossPhaseComponent] ========================================"));

    InitializePhases();
    InitialiseFromData();

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

    CurrentPower = FMath::Clamp(CurrentPower + PhaseData->PassivePowerPerSecond * DeltaTime, 0.f, PhaseData->MaxPower);
    
    UpdatePassivePower(DeltaTime);
    TryTriggerShout();

    if (StateTimeRemaining > 0.f)
    {
        StateTimeRemaining -= DeltaTime;
        if (StateTimeRemaining <= 0.f)
        {
            switch (CombatState)
            {
            case EBossCombatState::Intro:
            case EBossCombatState::Recovery:
                SetCombatState(EBossCombatState::Idle);
                break;
            case EBossCombatState::Pattern:
                SetCombatState(EBossCombatState::Recovery);
                StateTimeRemaining = PendingRecoveryDuration;
                PendingRecoveryDuration = 0.f;
                break;
            case EBossCombatState::Shout:
                SetCombatState(EBossCombatState::Idle);
                break;
            default:
                break;
            }
        }
    }


    if (CombatState == EBossCombatState::Idle && PhaseData->ShoutTriggerRatio > 0.f && PhaseData->MaxPower > 0.f)
    {
        const float PowerRatio = CurrentPower / PhaseData->MaxPower;
        if (PowerRatio >= PhaseData->ShoutTriggerRatio)
        {
            EnterShoutState();
        }
    }
    
    if (State == EBossBattleState::Dormant || State == EBossBattleState::Dead)
    {
        return;
    }

    TickState(DeltaTime);
}
void UCEnemyBossPhaseComponent::SetPhaseData(UBossPhaseDataAsset* InPhaseData)
{
    if (PhaseData == InPhaseData)
    {
        return;
    }
    
    PhaseData = InPhaseData;
    InitialiseFromData();
}

void UCEnemyBossPhaseComponent::RefreshPhaseFromHealth(float CurrentHealth, float MaxHealth)
{
    if (!PhaseData || PhaseData->Phases.IsEmpty())
    {
        return;
    }
  
    const float HealthRatio = (MaxHealth > SMALL_NUMBER) ? CurrentHealth / MaxHealth : 0.f;
    int32 TargetIndex = 0;
    
    for (int32 Index = 0; Index < PhaseData->Phases.Num(); ++Index)
    {
        if (HealthRatio <= PhaseData->Phases[Index].EnterHealthRatio + KINDA_SMALL_NUMBER)
        {
            TargetIndex = Index;
        }
    }
    
    if (CombatState == EBossCombatState::None)
    {
        HandlePhaseTransition(TargetIndex);
        return;
    }
    
    if (TargetIndex != CurrentPhaseIndex)
    {
        HandlePhaseTransition(TargetIndex);
    }
}

const FBossPhaseDefinition* UCEnemyBossPhaseComponent::GetCurrentPhaseDefinition() const
{
        return ResolvePhaseDefinition(CurrentPhaseIndex);
}

float UCEnemyBossPhaseComponent::GetCurrentPowerRatio() const
{
    if (!PhaseData || PhaseData->MaxPower <= 0.f)
        return 0.f;
    
    return CurrentPower / PhaseData->MaxPower;
}


void UCEnemyBossPhaseComponent::StartBattle(bool bSkipIntro)
{
    if (bBattleStarted)
    {
        return;
    }

    bBattleStarted = true;

    // AI 추적 활성화
    if (AActor* Owner = GetOwner())
    {
        if (ABossAIController* BossAI = Cast<ABossAIController>(Cast<APawn>(Owner)->GetController()))
        {
            BossAI->SetChaseEnabled(true);
            UE_LOG(LogTemp, Warning, TEXT("[BossPhaseComponent] Chase enabled on battle start"));
        }
    }
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

void UCEnemyBossPhaseComponent::ResetBattleState()
{
    UE_LOG(LogTemp, Log, TEXT("[BossPhaseComponent] ResetBattleState"));
    
    bBattleStarted = false;
    State = EBossBattleState::Dormant;
    CurrentPower = 0.f;
    StateTimeRemaining = 0.f;
    PendingRecoveryDuration = 0.f;
    CurrentPatternIndex = INDEX_NONE;
    ForcedPatternId = NAME_None;
    PatternCooldowns.Empty();
    ActivePatternId = NAME_None;
    
    if (PhaseData)
    {
        InitializePhases();
        InitialiseFromData();
            
        ShoutTriggerPower = PhaseData->MaxPower * PhaseData->ShoutTriggerRatio;
        CurrentPower = FMath::Clamp(CurrentPower, 0.f, PhaseData->MaxPower);
    }
    else
    {
        PhaseOrder.Reset();
        CurrentPhaseIndex = INDEX_NONE;
        ShoutTriggerPower = 0.f;
    }
    
    if (CachedHealth.IsValid())
    {
        const float MaxHealth = CachedHealth->GetMaxHealth();
        if (MaxHealth > KINDA_SMALL_NUMBER)
        {
            const float HealthRatio = CachedHealth->GetHealth() / MaxHealth;
            EvaluatePhaseByHealth(HealthRatio);
        }
    }
    
    SetCombatState(EBossCombatState::None);
    StateTimeRemaining = 0.f;
    PendingRecoveryDuration = 0.f;
}

void UCEnemyBossPhaseComponent::ForceNextPattern(FName PatternId)
{
    if (PatternId.IsNone() || !PhaseData)
    {
        return;
    }

    ForcedPatternId = PatternId;
}

void UCEnemyBossPhaseComponent::AddPower(float PowerDelta)
{
    if (!PhaseData)
    {
        return;
    }
  
    if (FMath::IsNearlyZero(PowerDelta))
    {
        return;
    }
  
    float AdjustedDelta = PowerDelta;
   if (const FBossPhaseDefinition* PhaseDefinition = GetCurrentPhaseDefinition())
    {
        if (PowerDelta >= 0.f)
        {
            AdjustedDelta *= FMath::Max(0.f, PhaseDefinition->PowerGainMultiplier);
        }
        else
        {
            AdjustedDelta *= FMath::Max(0.f, PhaseDefinition->PowerDrainMultiplier);
        }
    }
 
    CurrentPower = FMath::Clamp(CurrentPower + AdjustedDelta, 0.f, PhaseData->MaxPower);
}

void UCEnemyBossPhaseComponent::NotifyHitByPlayer(float PowerScale)
{
    if (!PhaseData)
        return;
    const float Scale = FMath::Max(0.f, PowerScale);
    const float BonusPower = PhaseData->PowerGainOnHit * Scale;
    if (BonusPower > 0.f)
    {
        AddPower(BonusPower);
    }
}


bool UCEnemyBossPhaseComponent::TryStartPattern(FBossPatternRuntime& OutRuntimeInfo)
{
    OutRuntimeInfo = {};
  
    const FBossPhaseDefinition* PhaseDefinition = GetCurrentPhaseDefinition();
    if (!PhaseDefinition || CombatState != EBossCombatState::Idle)
    {
        return false;
    }
   
    TArray<const FBossPatternDefinition*> Candidates;
    float TotalWeight = 0.f;
    
    for (const FBossPatternDefinition& Pattern : PhaseDefinition->Patterns)
    {
        if (IsPatternReady(Pattern))
        {
            Candidates.Add(&Pattern);
            TotalWeight += Pattern.Weight;
        }
    }
   
    if (Candidates.IsEmpty() || TotalWeight <= 0.f)
    {
        return false;
    }
   
    const float Roll = FMath::FRandRange(0.f, TotalWeight);
    float Accumulated = 0.f;
    const FBossPatternDefinition* SelectedPattern = Candidates.Last();
    
    for (const FBossPatternDefinition* Candidate : Candidates)
    {
        Accumulated += Candidate->Weight;
        if (Roll <= Accumulated)
        {
            SelectedPattern = Candidate;
            break;
        }
    }
  
    const float EffectiveCost = GetEffectivePowerCost(*SelectedPattern);
    if (EffectiveCost > 0.f)
    {
        CurrentPower = FMath::Max(0.f, CurrentPower - EffectiveCost);
    }
   
    const float BaseRecovery = PhaseDefinition->BaseRecoveryDuration;
    PendingRecoveryDuration = BaseRecovery + SelectedPattern->RecoveryTime;
    StateTimeRemaining = SelectedPattern->ExecutionTime;
    PatternCooldowns.FindOrAdd(SelectedPattern->PatternId) = SelectedPattern->Cooldown;
    ActivePatternId = SelectedPattern->PatternId;
   
    OutRuntimeInfo.PatternId = SelectedPattern->PatternId;
    OutRuntimeInfo.ExecutionTime = SelectedPattern->ExecutionTime;
    OutRuntimeInfo.RecoveryTime = SelectedPattern->RecoveryTime;
    OutRuntimeInfo.bIsClimax = SelectedPattern->bIsClimax;
    
    SetCombatState(EBossCombatState::Pattern, SelectedPattern->PatternId);
    return true;
}


void UCEnemyBossPhaseComponent::NotifyPatternFinished(bool bWasSuccessful)
{
    if (!PhaseData)
    {
        return;
    }
    
    if (CombatState != EBossCombatState::Pattern)
    {
        return;
    }
  

    /*
    const FBossPhaseDefinition* PhaseDefinition = GetCurrentPhaseDefinition();
    if (!PhaseDefinition)
    {
        return;
    }
 
    const FBossPatternDefinition* PatternDefinition = PhaseDefinition->Patterns.FindByPredicate([
      PatternId = ActivePatternId
    ](const FBossPatternDefinition& Definition)
    {
       return Definition.PatternId == PatternId;
    });

    if (bWasSuccessful && PatternDefinition)
    {
        const float Reward = GetEffectivePowerReward(*PatternDefinition);
        if (Reward > 0.f)
        {
            CurrentPower = FMath::Clamp(CurrentPower + Reward, 0.f, PhaseData->MaxPower);
        }
    }
    */
    // 즉시 리커버리로 전환될 수 있도록 남은 시간을 0으로 처리.
    StateTimeRemaining = 0.f;
}


void UCEnemyBossPhaseComponent::ApplyInterruptPenalty()
{
    if (!PhaseData)
        return;
    AddPower(-PhaseData->PowerLossOnInterrupt);
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
    if (!PhaseData)
    {
        UE_LOG(LogTemp, Error, TEXT("[PhaseComponent] GetCurrentPhase - PhaseData is NULL!"));
        return GDummyPhase;
    }
    
    if (!PhaseData->Phases.IsValidIndex(CurrentPhaseIndex))
    {
        UE_LOG(LogTemp, Error, TEXT("[PhaseComponent] GetCurrentPhase - CurrentPhaseIndex(%d) is INVALID! Phases.Num=%d"), 
            CurrentPhaseIndex, PhaseData->Phases.Num());
        return GDummyPhase;
    }
    
    UE_LOG(LogTemp, Log, TEXT("[PhaseComponent] GetCurrentPhase - CurrentPhaseIndex=%d, Patterns.Num=%d"), 
        CurrentPhaseIndex, PhaseData->Phases[CurrentPhaseIndex].Patterns.Num());
    return PhaseData->Phases[CurrentPhaseIndex];
}

void UCEnemyBossPhaseComponent::InitialiseFromData()
{
    CurrentPower = 0.f;
    CombatState = EBossCombatState::None;
    ActivePatternId = NAME_None;
    StateTimeRemaining = 0.f;
    PendingRecoveryDuration = 0.f;
    PatternCooldowns.Empty();
   
    if (!PhaseData)
    {
        UE_LOG(LogTemp, Error, TEXT("[PhaseComponent] InitialiseFromData - PhaseData is NULL!"));
        CurrentPhaseIndex = INDEX_NONE;
        return;
    }
 
    UE_LOG(LogTemp, Warning, TEXT("[PhaseComponent] InitialiseFromData - PhaseData valid, Phases.Num=%d"), PhaseData->Phases.Num());
    CurrentPhaseIndex = 0;
    if (!PhaseData->Phases.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[PhaseComponent] InitialiseFromData - Calling HandlePhaseTransition(0)"));
        HandlePhaseTransition(0);
    }
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

void UCEnemyBossPhaseComponent::HandlePhaseTransition(int32 NewPhaseIndex)
{
    UE_LOG(LogTemp, Warning, TEXT("[BossPhaseComponent] HandlePhaseTransition(%d) - bBattleStarted=%s"), NewPhaseIndex, bBattleStarted ? TEXT("TRUE") : TEXT("FALSE"));
    
    if (!PhaseData || !PhaseData->Phases.IsValidIndex(NewPhaseIndex))
    {
        return;
    }
  
    CurrentPhaseIndex = NewPhaseIndex;
    CurrentPower = FMath::Clamp(CurrentPower, 0.f, PhaseData->MaxPower);
    PatternCooldowns.Empty();
    ActivePatternId = NAME_None;
    PendingRecoveryDuration = 0.f;
  
    const FBossPhaseDefinition& PhaseDefinition = PhaseData->Phases[CurrentPhaseIndex];
    StateTimeRemaining = PhaseDefinition.IntroDuration;
    SetCombatState(EBossCombatState::Intro);
    
    // 전투가 시작된 경우에만 브로드캐스트
    if (bBattleStarted)
    {
        OnBossPhaseChanged.Broadcast(CurrentPhaseIndex, PhaseDefinition.PhaseName);
    }
}

void UCEnemyBossPhaseComponent::SetCombatState(EBossCombatState NewState, FName InPatternId)
{
    CombatState = NewState;
    ActivePatternId = InPatternId;
    OnBossStateChanged.Broadcast(CombatState, ActivePatternId);
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
    PatternCooldowns.Empty();
}

void UCEnemyBossPhaseComponent::EnterShoutState()
{
    const FBossPhaseDefinition* PhaseDefinition = GetCurrentPhaseDefinition();
    if (!PhaseDefinition || PhaseDefinition->ShoutMetaId.IsNone())
    {
        return;
    }
 
    CurrentPower = 0.f;
    StateTimeRemaining = PhaseDefinition->ShoutDuration;
    SetCombatState(EBossCombatState::Shout, PhaseDefinition->ShoutMetaId);
    
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
    // ExecutingPattern 상태 안전장치
    if (State == EBossBattleState::ExecutingPattern)
    {
        // 패턴이 설정되지 않았거나 이미 종료된 경우 강제 advance
        if (CurrentPatternIndex == INDEX_NONE)
        {
            UE_LOG(LogTemp, Warning, TEXT("[PhaseComponent] Safety: ExecutingPattern with no pattern - Force advancing"));
            AdvanceState();
            return;
        }
    }
    
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
            UE_LOG(LogTemp, Warning, TEXT("[PhaseComponent] AdvanceState - Recover: Selecting next pattern..."));
            const int32 NextPattern = SelectNextPatternIndex();
            if (NextPattern != INDEX_NONE)
            {
                UE_LOG(LogTemp, Warning, TEXT("[PhaseComponent] AdvanceState - Selected pattern index: %d"), NextPattern);
                BeginPattern(NextPattern);
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("[PhaseComponent] AdvanceState - No pattern selected! Waiting 0.5s..."));
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
    for (auto It = PatternCooldowns.CreateIterator(); It; ++It)
    {
        It->Value = FMath::Max(0.f, It->Value - DeltaTime);
        if (It->Value <= SMALL_NUMBER)
        {
            It.RemoveCurrent();
        }
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

const FBossPhaseDefinition* UCEnemyBossPhaseComponent::ResolvePhaseDefinition(int32 PhaseIndex) const
{
    if (!PhaseData || !PhaseData->Phases.IsValidIndex(PhaseIndex))
    {
        return nullptr;
    }
    return &PhaseData->Phases[PhaseIndex];
}

bool UCEnemyBossPhaseComponent::IsPatternReady(const FBossPatternDefinition& PatternDef) const
{
    if (!PhaseData)
    {
        return false;
    }
   
    const float* CooldownPtr = PatternCooldowns.Find(PatternDef.PatternId);
    if (CooldownPtr && *CooldownPtr > 0.f)
    {
        return false;
    }
  
    const float EffectiveCost = GetEffectivePowerCost(PatternDef);
    if (EffectiveCost > 0.f && CurrentPower < EffectiveCost)
    {
        return false;
    }
    
    if (PatternDef.RequiredPower > 0.f && CurrentPower < PatternDef.RequiredPower)
    {
        return false;
    }
    
    return true;
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
    
    const float Drain = (Pattern.PowerCost + Pattern.RequiredPower) * GetCurrentPhase().PowerDrainMultiplier;
    if (Drain > 0.f)
    {
        ApplyPowerDelta(-Drain);
    }

    ForcedPatternId = NAME_None;


    // 패턴 시작 시 쿨다운 맵에 추가
    PatternCooldowns.FindOrAdd(Pattern.PatternId) = Pattern.Cooldown;

    // ExecutionTime이 0 이하면 최소값 보장
    float ExecutionDuration = Pattern.ExecutionTime;
    if (ExecutionDuration <= 0.f)
    {
        ExecutionDuration = 0.5f; 
        UE_LOG(LogTemp, Warning, TEXT("[PhaseComponent] Pattern %s has ExecutionTime <= 0, using fallback: 0.5s"), 
            *Pattern.PatternId.ToString());
    }

    EnterState(EBossBattleState::ExecutingPattern, ExecutionDuration);
}

void UCEnemyBossPhaseComponent::FinishPattern(bool bInterrupted)
{
    // 재진입 방지
    static bool bIsFinishing = false;
    if (bIsFinishing)
    {
        UE_LOG(LogTemp, Warning, TEXT("[PhaseComponent] FinishPattern re-entry blocked"));
        return;
    }
    
    bIsFinishing = true;
    
    if (!PhaseData)
    {
        bIsFinishing = false;
        return;
    }

    if (CurrentPatternIndex == INDEX_NONE)
    {
        UE_LOG(LogTemp, Warning, TEXT("[PhaseComponent] FinishPattern called with no pattern - Forcing Recover"));
        
        if (State == EBossBattleState::ExecutingPattern)
        {
            UE_LOG(LogTemp, Warning, TEXT("[PhaseComponent] Force advancing from stuck ExecutingPattern"));
        }
        
        EnterState(EBossBattleState::Recover, GetCurrentPhase().BaseRecoveryDuration);
        bIsFinishing = false;
        return;
    }

    const FBossPatternDefinition& Pattern = GetCurrentPhase().Patterns[CurrentPatternIndex];
    if (!bInterrupted)
    {
        PatternCooldowns.FindOrAdd(Pattern.PatternId) = Pattern.Cooldown;
        ApplyPowerDelta(Pattern.PowerReward);
    }
    else// if (PhaseData->PowerLossOnInterrupt > 0.f)
    {
        PatternCooldowns.FindOrAdd(Pattern.PatternId) = Pattern.Cooldown * 0.5f;
        ApplyPowerDelta(-PhaseData->PowerLossOnInterrupt);
    }

    OnPatternFinished.Broadcast(CurrentPhaseIndex, Pattern.PatternId, Pattern, CurrentPower);

    CurrentPatternIndex = INDEX_NONE;
    EnterState(EBossBattleState::Recover, FMath::Max(GetCurrentPhase().BaseRecoveryDuration, Pattern.RecoveryTime));
    
    bIsFinishing = false;
}

int32 UCEnemyBossPhaseComponent::SelectNextPatternIndex() const
{
    if (!PhaseData)
    {
        UE_LOG(LogTemp, Error, TEXT("[PhaseComponent] SelectNextPatternIndex - PhaseData is NULL!"));
        return INDEX_NONE;
    }

    const TArray<FBossPatternDefinition>& Patterns = GetCurrentPhase().Patterns;
    UE_LOG(LogTemp, Warning, TEXT("[PhaseComponent] SelectNextPatternIndex - Patterns.Num=%d, CurrentPhaseIndex=%d"), 
        Patterns.Num(), CurrentPhaseIndex);
    
    if (Patterns.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("[PhaseComponent] SelectNextPatternIndex - No patterns available!"));
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

    const FBossPatternDefinition& Pattern = Patterns[PatternIndex];

    const float* CooldownPtr = PatternCooldowns.Find(Pattern.PatternId);
    if (CooldownPtr && *CooldownPtr > 0.f)
    {
        return false;
    }

    if (CurrentPower < Pattern.RequiredPower)
    {
        return false;
    }

    return true;
}

float UCEnemyBossPhaseComponent::GetEffectivePowerCost(const FBossPatternDefinition& PatternDef) const
{
 
    const FBossPhaseDefinition* PhaseDefinition = GetCurrentPhaseDefinition();
     if (!PhaseDefinition)
         {
                 return PatternDef.PowerCost;
         }
   
     return PatternDef.PowerCost * FMath::Max(0.f, PhaseDefinition->PowerDrainMultiplier);
     
    return 0.f; // 임시 반환값 <- 위에 수정되면 지워야함.
}

float UCEnemyBossPhaseComponent::GetEffectivePowerReward(const FBossPatternDefinition& PatternDef) const
{
   
    const FBossPhaseDefinition* PhaseDefinition = GetCurrentPhaseDefinition();
    if (!PhaseDefinition)
    {
        return PatternDef.PowerReward;
    }
   
    return PatternDef.PowerReward * FMath::Max(0.f, PhaseDefinition->PowerGainMultiplier);

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