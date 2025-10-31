// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CFuryGaugeComponent.generated.h"


USTRUCT(BlueprintType)
struct FFuryGaugeTier
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fury")
    int32 MinStacks = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fury")
    int32 MaxStacks = 3;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fury", meta=(ClampMin="0"))
    float Duration = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fury")
    float TotalDamage = 100.f;
};

// ─ 이벤트 ─
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams (FOnFuryStacksChanged,    int32, NewStacks, int32, MaxStacks);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnFuryEffectStarted,   int32, TierIdx, float, Duration, float, TotalDamage, int32, InitialStacks);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams (FOnFuryEffectEnded,     bool, bCanceled, float, TimeRemaining);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams (FOnFuryEffectTick,      float, TimeRemaining, float, TotalDuration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam  (FOnFuryFinisherTriggered, float, FinisherDamage); // 1타

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class POSISONFROG_API UCFuryGaugeComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCFuryGaugeComponent();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fury", meta=(ClampMin="1"))
    int32 MaxStacks = 10;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Fury")
    int32 CurrentStacks = 0;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fury")
    bool bBlockStackWhileActive = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fury", meta=(ClampMin="0"))
    float EffectUITickInterval = 0.05f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fury")
    TArray<FFuryGaugeTier> Tiers;

    /** 10칸 발동 종료 시 ‘망치 내려찍기’ 일격 피해량 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fury")
    float FinisherDamageAtMaxStacks = 250.f;

public:
    UFUNCTION(BlueprintCallable, Category="Fury")
    void AddStack(int32 Amount = 1);

    UFUNCTION(BlueprintCallable, Category="Fury")
    bool ActivateEffect();     // 발동(전량 소모)

    UFUNCTION(BlueprintCallable, Category="Fury")
    bool CancelEffect();       // 즉시 취소(10칸이면 즉시 피니시)

    UFUNCTION(BlueprintPure,   Category="Fury")
    bool IsEffectActive() const { return bEffectActive; }

    UFUNCTION(BlueprintPure,   Category="Fury")
    float GetEffectTimeRemaining() const;

    UFUNCTION(BlueprintPure,   Category="Fury")
    float GetEffectTotalDuration() const { return ActiveTotalDuration; }

    UFUNCTION(BlueprintPure,   Category="Fury")
    int32 GetCurrentFury() const { return CurrentStacks; }

    UFUNCTION(BlueprintCallable, Category="Fury")
    void SetFury(int32 NewStacks);

    UFUNCTION(BlueprintPure,   Category="Fury")
    int32 FindTierIndexForStacks(int32 Stacks) const;

public: // 델리게이트
    UPROPERTY(BlueprintAssignable, Category="Fury") FOnFuryStacksChanged      OnStacksChanged;
    UPROPERTY(BlueprintAssignable, Category="Fury") FOnFuryEffectStarted      OnEffectStarted;
    UPROPERTY(BlueprintAssignable, Category="Fury") FOnFuryEffectEnded        OnEffectEnded;
    UPROPERTY(BlueprintAssignable, Category="Fury") FOnFuryEffectTick         OnEffectTick;
    UPROPERTY(BlueprintAssignable, Category="Fury") FOnFuryFinisherTriggered  OnFinisherTriggered;

protected:
    virtual void BeginPlay() override;

private:
    void EndEffectInternal(bool bCanceled, float CanceledRemainTime = 0.f);
    void EffectUITick();

private:
    bool  bEffectActive = false;
    float ActiveEndTime = -1.f;
    float ActiveTotalDuration = 0.f;
    float ActiveTotalDamage = 0.f;
    int32 ActiveTierIndex = -1;

    int32 InitialStacksAtActivation = 0;  // 발동 당시 칸수(피니시 판정)
    bool  bFinisherTriggered = false;     // 피니시 1회 보장

    FTimerHandle TimerHandle_EffectEnd;
    FTimerHandle TimerHandle_EffectUITick;
};

