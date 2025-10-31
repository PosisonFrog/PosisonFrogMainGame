#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "03_Combat/Boss/BossPhaseDataAsset.h"
#include "CEnemyBossPhaseComponent.generated.h"

UENUM(BlueprintType)
enum class EBossCombatState : uint8
{
    None,
    Intro,
    Idle,
    Pattern,
    Recovery,
    Shout
};

UENUM(BlueprintType)
enum class EBossBattleState : uint8
{
    Dormant,
    Intro,
    PhaseIntro,
    ExecutingPattern,
    Recover,
    Shout,
    Dead
};


USTRUCT(BlueprintType)
struct FBossPatternRuntime
{
    GENERATED_BODY()
    
    UPROPERTY(BlueprintReadOnly)
    FName PatternId = NAME_None;
    
    UPROPERTY(BlueprintReadOnly)
    float ExecutionTime = 0.f;
    
    UPROPERTY(BlueprintReadOnly)
    float RecoveryTime = 0.f;
    
    UPROPERTY(BlueprintReadOnly)
    bool bIsClimax = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBossPhaseChanged, int32, NewPhaseIndex, FName, PhaseName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBossStateChanged, EBossCombatState, NewState, FName, PatternId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBossPhaseChangedSignature, int32, PhaseIndex, const FBossPhaseDefinition&, PhaseData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FBossPatternEventSignature, int32, PhaseIndex, FName, PatternId, const FBossPatternDefinition&, PatternData, float, RemainingPower);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBossStateChangedSignature, EBossBattleState, NewState, EBossBattleState, OldState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FBossShoutEventSignature, int32, PhaseIndex, FName, ShoutId, float, Duration);


class UCEnemyHealthComponent;

UCLASS(ClassGroup=("Boss"), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class POSISONFROG_API UCEnemyBossPhaseComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCEnemyBossPhaseComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    
    /** 보스 페이즈 데이터를 설정합니다. BeginPlay 이전에 호출하면 초기화 시 자동으로 반영됩니다. */
    UFUNCTION(BlueprintCallable, Category="Boss")
    void SetPhaseData(UBossPhaseDataAsset* InPhaseData);
    
    /** 체력 정보를 바탕으로 현재 페이즈를 재계산합니다. */
    UFUNCTION(BlueprintCallable, Category="Boss")
    void RefreshPhaseFromHealth(float CurrentHealth, float MaxHealth);
    
    /** 현재 페이즈 정의를 반환합니다. 유효하지 않으면 nullptr. */
    //UFUNCTION(BlueprintPure, Category="Boss")
    const FBossPhaseDefinition* GetCurrentPhaseDefinition() const;
    
    /** 현재 파워 값을 반환합니다. */
    UFUNCTION(BlueprintPure, Category="Boss")
    float GetCurrentPowerRatio() const;
    
    /** 외부 요인으로 파워를 증감시킵니다. */
    UFUNCTION(BlueprintCallable, Category="Boss")
    void AddPower(float PowerDelta);
    
    /** 플레이어의 공격에 피격되었을 때 호출하여 파워를 획득합니다. */
    UFUNCTION(BlueprintCallable, Category="Boss")
    void NotifyHitByPlayer(float PowerScale = 1.f);
    
    /** 패턴 수행을 요청합니다. 성공 시 실행 정보를 반환합니다. */
    UFUNCTION(BlueprintCallable, Category="Boss")
    bool TryStartPattern(FBossPatternRuntime& OutRuntimeInfo);
    
    /** 패턴 수행 완료를 통지합니다. */
    UFUNCTION(BlueprintCallable, Category="Boss")
    void NotifyPatternFinished(bool bWasSuccessful);
    
    /** 패턴이 강제 중단되었을 때 호출하여 패널티를 부여합니다. */
    UFUNCTION(BlueprintCallable, Category="Boss")
    void ApplyInterruptPenalty();
    
    /** 현재 보스 상태 */
    UFUNCTION(BlueprintPure, Category="Boss")
    EBossCombatState GetCombatState() const { return CombatState; }
    
    /** 현재 진행 중인 패턴 ID */
    UFUNCTION(BlueprintPure, Category="Boss")
    FName GetActivePatternId() const { return ActivePatternId; }
    
    /** 페이즈 변경 델리게이트 */
    UPROPERTY(BlueprintAssignable, Category="Boss")
    FOnBossPhaseChanged OnBossPhaseChanged;
    
    /** 상태 변경 델리게이트 */
    UPROPERTY(BlueprintAssignable, Category="Boss")
    FOnBossStateChanged OnBossStateChanged;
    
    /** 전투 개시(인트로 종료 후 호출) */
    UFUNCTION(BlueprintCallable, Category="Boss")
    void StartBattle(bool bSkipIntro = false);

    /** 특정 패턴을 강제로 예약 */
    UFUNCTION(BlueprintCallable, Category="Boss")
    void ForceNextPattern(FName PatternId);

    UFUNCTION(BlueprintCallable, Category="Boss")
    void ConsumePower(float Amount);

    UFUNCTION(BlueprintPure, Category="Boss")
    float GetCurrentPower() const { return CurrentPower; }

    UFUNCTION(BlueprintPure, Category="Boss")
    int32 GetCurrentPhaseIndex() const { return CurrentPhaseIndex; }

    UFUNCTION(BlueprintPure, Category="Boss")
    EBossBattleState GetCurrentState() const { return State; }

    UFUNCTION(BlueprintPure, Category="Boss")
    bool IsBattleStarted() const { return bBattleStarted; }

    UFUNCTION(BlueprintPure, Category="Boss")
    const FBossPhaseDefinition& GetCurrentPhase() const;
    
    
    /** 설계 데이터 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss")
    TObjectPtr<const UBossPhaseDataAsset> PhaseData;

    /** 체력 변화 시 페이즈 변경 알림 */
    UPROPERTY(BlueprintAssignable, Category="Boss|Event")
    FBossPhaseChangedSignature OnPhaseChanged;

    /** 패턴 시작/종료 이벤트 */
    UPROPERTY(BlueprintAssignable, Category="Boss|Event")
    FBossPatternEventSignature OnPatternStarted;

    UPROPERTY(BlueprintAssignable, Category="Boss|Event")
    FBossPatternEventSignature OnPatternFinished;

    /** 상태 전환 알림 */
    UPROPERTY(BlueprintAssignable, Category="Boss|Event")
    FBossStateChangedSignature OnStateChanged;

    /** 메타 샤우트 발생 */
    UPROPERTY(BlueprintAssignable, Category="Boss|Event")
    FBossShoutEventSignature OnShoutStarted;

    UPROPERTY(BlueprintAssignable, Category="Boss|Event")
    FBossShoutEventSignature OnShoutFinished;

protected:
    void InitialiseFromData();
    void InitializePhases();
    void HandlePhaseTransition(int32 NewPhaseIndex);
    void SetCombatState(EBossCombatState NewState, FName InPatternId = NAME_None);
    void EvaluatePhaseByHealth(float HealthRatio);
    void EnterPhase(int32 PhaseIndex);
    void ResetPhaseRuntime();

    void EnterShoutState();
    void EnterState(EBossBattleState NewState, float Duration = 0.f);
    void TickState(float DeltaTime);
    void AdvanceState();
    void UpdateCooldowns(float DeltaTime);
    void UpdatePassivePower(float DeltaTime);
    void TryTriggerShout();
    void ApplyPowerDelta(float Delta);

    const FBossPhaseDefinition* ResolvePhaseDefinition(int32 PhaseIndex) const;
    bool IsPatternReady(const FBossPatternDefinition& PatternDef) const;
    void BeginPattern(int32 PatternIndex);
    void FinishPattern(bool bInterrupted);
    int32 SelectNextPatternIndex() const;
    bool CanUsePattern(int32 PatternIndex) const;

    float GetEffectivePowerCost(const FBossPatternDefinition& PatternDef) const;
    float GetEffectivePowerReward(const FBossPatternDefinition& PatternDef) const;
    
    UFUNCTION()
    void HandleHealthChanged(float Current, float Max);

    UFUNCTION()
    void HandleDeath(AActor* DeadActor);

protected:
    TWeakObjectPtr<UCEnemyHealthComponent> CachedHealth;
    
    UPROPERTY(BlueprintReadOnly, Category="Boss", meta=(AllowPrivateAccess="true"))
    EBossCombatState CombatState;
    
    bool bBattleStarted = false;
    int32 CurrentPhaseIndex = INDEX_NONE;
    int32 CurrentPatternIndex = INDEX_NONE;
    float CurrentPower = 0.f;

    EBossBattleState State = EBossBattleState::Dormant;
    float StateTimeRemaining = 0.f;

    UPROPERTY(BlueprintReadOnly, Category="Boss", meta=(AllowPrivateAccess="true"))
    FName ActivePatternId;

    UPROPERTY(BlueprintReadOnly, Category="Boss", meta=(AllowPrivateAccess="true"))
    float PendingRecoveryDuration;
    
    UPROPERTY(Transient)
    TMap<FName, float> PatternCooldowns;
    TArray<int32> PhaseOrder;

    FName ForcedPatternId = NAME_None;

    float ShoutTriggerPower = 0.f;
};
