#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "ComboStackComponent.generated.h"

UENUM(BlueprintType)
enum class EComboRank : uint8
{
    D,
    C,
    B,
    A,
    S
};

UENUM(BlueprintType)
enum class EUltState : uint8
{
    Normal,
    Ready,
    Casting
};

UENUM(BlueprintType)
enum class ECSCResetReason : uint8
{
    None,
    Reset_BA_Rush,
    Reset_Stagger,
    Reset_Respawn
};

USTRUCT(BlueprintType)
struct FComboStackConfig : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CSC")
    int32 MaxStacks = 30;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CSC")
    int32 GainPerHit = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CSC")
    float DecayStartDelay = 8.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CSC")
    float DemoteInterval = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CSC")
    EComboRank DemoteFloorRank = EComboRank::C;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CSC")
    bool bSameFrameGuard = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CSC")
    float SameFrameTolerance = 0.01f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CSC")
    float SameFrameWindow = 0.25f;
};

USTRUCT(BlueprintType)
struct FComboRankThreshold : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CSC")
    EComboRank Rank = EComboRank::D;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CSC")
    int32 MinCSC = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CSC")
    int32 MaxCSC = 5;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCSCChanged, int32, NewCSC);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnComboRankChanged, EComboRank, OldRank, EComboRank, NewRank);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUltStateChanged, EUltState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFirstSRankAchieved);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class POSISONFROG_API UComboStackComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UComboStackComponent();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UFUNCTION(BlueprintCallable, Category="CSC")
    void OnDirectHit(FName AttackId, float HitWorldTime);

    UFUNCTION(BlueprintCallable, Category="CSC")
    void OnReset(ECSCResetReason Reason);

    UFUNCTION(BlueprintCallable, Category="CSC")
    bool CanCastUlt() const;

    UFUNCTION(BlueprintCallable, Category="CSC")
    void OnUltStarted(FName UltId);

    UFUNCTION(BlueprintCallable, Category="CSC")
    void OnUltEnded(FName UltId);

    UFUNCTION(BlueprintPure, Category="CSC")
    int32 GetCurrentCSC() const { return CSC; }

    UFUNCTION(BlueprintPure, Category="CSC")
    EComboRank GetCurrentRank() const { return Rank; }

    UFUNCTION(BlueprintPure, Category="CSC")
    EUltState GetUltState() const { return UltState; }

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CSC|Data")
    TObjectPtr<UDataTable> ConfigTable = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CSC|Data")
    TObjectPtr<UDataTable> RankThresholdTable = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CSC")
    FComboStackConfig Config;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CSC")
    TArray<FComboRankThreshold> RankThresholds;

    UPROPERTY(BlueprintAssignable, Category="CSC")
    FOnCSCChanged OnCSCChanged;

    UPROPERTY(BlueprintAssignable, Category="CSC")
    FOnComboRankChanged OnRankChanged;

    UPROPERTY(BlueprintAssignable, Category="CSC")
    FOnUltStateChanged OnUltStateChanged;

    UPROPERTY(BlueprintAssignable, Category="CSC")
    FOnFirstSRankAchieved OnFirstSRankAchieved;

protected:
    void RefreshConfigFromTable();
    void RefreshRanksFromTable();

    void HandleDecay(float CurrentTime);
    void EnterDecay(float CurrentTime);
    void ExitDecay();
    bool TryDemoteOneStep(float CurrentTime);

    void SetCSC(int32 NewValue, bool bBroadcast = true, bool bForceBroadcast = false);
    void SetRank(EComboRank NewRank, bool bForceBroadcast = false);
    void SetUltState(EUltState NewState);

    EComboRank EvaluateRankForCSC(int32 Value) const;
    int32 GetMaxCSCForRank(EComboRank InRank) const;
    int32 GetRankIndex(EComboRank InRank) const;

    void ResetTimers();
    void ResetInternal(bool bResetCSC, bool bBroadcast);

    void CleanupSameFrameRecords(float CurrentTime);

private:
    int32 CSC = 0;
    EComboRank Rank = EComboRank::D;
    float LastHitTime = -1.f;
    bool bIsDecaying = false;
    float LastDecayTime = -1.f;
    bool bHasPlayedFirstS = false;
    EUltState UltState = EUltState::Normal;

    TMap<FName, float> LastAttackHitTimes;
};

