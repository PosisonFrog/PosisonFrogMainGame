#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "03_Combat/Boss/BossPhaseDataAsset.h"
#include "CEnemyBossPhaseComponent.generated.h"

class UCEnemyHealthComponent;

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

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBossPhaseChangedSignature, int32, PhaseIndex, const FBossPhaseDefinition&, PhaseData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FBossPatternEventSignature, int32, PhaseIndex, FName, PatternId, const FBossPatternDefinition&, PatternData, float, RemainingPower);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBossStateChangedSignature, EBossBattleState, NewState, EBossBattleState, OldState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FBossShoutEventSignature, int32, PhaseIndex, FName, ShoutId, float, Duration);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class POSISONFROG_API UCEnemyBossPhaseComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCEnemyBossPhaseComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    /** 전투 개시(인트로 종료 후 호출) */
    UFUNCTION(BlueprintCallable, Category="Boss")
    void StartBattle(bool bSkipIntro = false);

    /** 특정 패턴을 강제로 예약 */
    UFUNCTION(BlueprintCallable, Category="Boss")
    void ForceNextPattern(FName PatternId);

    /** 파워 수급 */
    UFUNCTION(BlueprintCallable, Category="Boss")
    void AddPower(float Amount);

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
    void InitializePhases();
    void EvaluatePhaseByHealth(float HealthRatio);
    void EnterPhase(int32 PhaseIndex);
    void ResetPhaseRuntime();

    void EnterState(EBossBattleState NewState, float Duration = 0.f);
    void TickState(float DeltaTime);
    void AdvanceState();
    void UpdateCooldowns(float DeltaTime);
    void UpdatePassivePower(float DeltaTime);
    void TryTriggerShout();
    void ApplyPowerDelta(float Delta);

    void BeginPattern(int32 PatternIndex);
    void FinishPattern(bool bInterrupted);
    int32 SelectNextPatternIndex() const;
    bool CanUsePattern(int32 PatternIndex) const;

    UFUNCTION()
    void HandleHealthChanged(float Current, float Max);

    UFUNCTION()
    void HandleDeath();

protected:
    TWeakObjectPtr<UCEnemyHealthComponent> CachedHealth;

    bool bBattleStarted = false;
    int32 CurrentPhaseIndex = INDEX_NONE;
    int32 CurrentPatternIndex = INDEX_NONE;
    float CurrentPower = 0.f;

    EBossBattleState State = EBossBattleState::Dormant;
    float StateTimeRemaining = 0.f;

    TArray<int32> PhaseOrder;
    TArray<float> PatternCooldowns;

    FName ForcedPatternId = NAME_None;

    float ShoutTriggerPower = 0.f;
};
