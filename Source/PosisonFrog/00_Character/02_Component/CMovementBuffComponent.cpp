#include "CMovementBuffComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UCMovementBuffComponent::UCMovementBuffComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UCMovementBuffComponent::BeginPlay()
{
    Super::BeginPlay();

    if (const ACharacter* Ch = Cast<ACharacter>(GetOwner()))
    {
        MoveComp = Ch->GetCharacterMovement();
        if (MoveComp)
        {
            BaseMaxWalkSpeed = MoveComp->MaxWalkSpeed;
            CurrentMaxMultiplier = 1.f;
        }
    }
}

void UCMovementBuffComponent::SetBaseMaxWalkSpeed(float InBaseSpeed)
{
    BaseMaxWalkSpeed = FMath::Max(0.f, InBaseSpeed);
    RecomputeAndApply();
}

void UCMovementBuffComponent::AddSpeedBuff(float Multiplier, float DurationSeconds)
{
    if (!MoveComp || Multiplier <= 1.f || DurationSeconds <= 0.f)
        return;

    FActiveSpeedBuff Buff;
    Buff.Multiplier = Multiplier;
    Buff.ExpireTime = GetWorld()->GetTimeSeconds() + DurationSeconds;

    const int32 Index = ActiveBuffs.Add(Buff);

    GetWorld()->GetTimerManager().SetTimer(
        ActiveBuffs[Index].TimerHandle,
        FTimerDelegate::CreateWeakLambda(this, [this, Index]()
            {
                if (this) OnBuffExpired(Index);
            }),
        DurationSeconds, false
    );

    RecomputeAndApply();
}

void UCMovementBuffComponent::OnBuffExpired(int32 Index)
{
    if (!ActiveBuffs.IsValidIndex(Index))
        return;

    GetWorld()->GetTimerManager().ClearTimer(ActiveBuffs[Index].TimerHandle);
    ActiveBuffs[Index] = FActiveSpeedBuff(); // 빈 값으로
    RecomputeAndApply();
}

void UCMovementBuffComponent::RecomputeAndApply()
{
    const float Now = GetWorld()->GetTimeSeconds();

    float NewMax = 1.f;
    for (int32 i = ActiveBuffs.Num() - 1; i >= 0; --i)
    {
        const FActiveSpeedBuff& B = ActiveBuffs[i];
        if (B.ExpireTime <= 0.f || B.ExpireTime <= Now)
        {
            GetWorld()->GetTimerManager().ClearTimer(ActiveBuffs[i].TimerHandle);
            ActiveBuffs.RemoveAtSwap(i);
            continue;
        }
        NewMax = FMath::Max(NewMax, B.Multiplier);
    }

    CurrentMaxMultiplier = NewMax;

    if (MoveComp && BaseMaxWalkSpeed > 0.f)
        MoveComp->MaxWalkSpeed = BaseMaxWalkSpeed * CurrentMaxMultiplier;
}
