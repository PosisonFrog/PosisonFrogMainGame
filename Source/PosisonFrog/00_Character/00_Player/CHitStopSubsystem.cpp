// Fill out your copyright notice in the Description page of Project Settings.


#include "CHitStopSubsystem.h"

#include "NiagaraComponent.h"
#include "99_Util/CLog.h"

UCHitStopSubsystem::UCHitStopSubsystem()
{
}

void UCHitStopSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (bShowDebugMessages)
		CLog::Log(TEXT("[HitStopSubsystem] 초기화 완료"));
}

void UCHitStopSubsystem::Deinitialize()
{
	EndAllHitStops();

	if (bShowDebugMessages)
		CLog::Log(TEXT("[HitStopSubsystem] 종료"));
	
	Super::Deinitialize();
}


bool UCHitStopSubsystem::StartHitStop(AActor* TargetActor, float Duration, float TimeScale)
{
	if (!IsValid(TargetActor))
		return false;

	UWorld* World = GetWorld();
	if (!World)
		return false;

	TWeakObjectPtr<AActor> WeakTargetActor = TargetActor;
	
	if (ActorsInHitStop.Contains(WeakTargetActor))
	{
		if (bShowDebugMessages)
			CLog::Log(FString::Printf(TEXT("[HitStop] %s는 이미 히트 스탑 중이므로 무시"), *TargetActor->GetName()));
		return false;
	}

	Duration = FMath::Clamp(Duration, 0.01f, 1.0f);
	TimeScale = FMath::Clamp(TimeScale, 0.0f, 1.0f);

	if (!OriginalTimeDilations.Contains(WeakTargetActor))
	{
		OriginalTimeDilations.Add(WeakTargetActor, TargetActor->CustomTimeDilation);
	}

	ActorsInHitStop.Add(WeakTargetActor);
	TargetActor->CustomTimeDilation = TimeScale;

	TArray<UActorComponent*> Components;
	TargetActor->GetComponents(Components);

	for (UActorComponent* Component : Components)
	{
		if (!IsValid(Component))
			continue;

		if (UNiagaraComponent* NiagaraComp = Cast<UNiagaraComponent>(Component))
			NiagaraComp->SetFloatParameter(FName("TimeDilation"), TimeScale);
	}

	if (bShowDebugMessages)
		CLog::Log(FString::Printf(TEXT("[HitStop] %s에 커스텀 히트 스탑 시작 (컴포넌트 %d개) - 지속시간: %.3f초, 시간배율: %.3f"), *TargetActor->GetName(), Components.Num(), Duration, TimeScale));

	FTimerHandle& ActorTimer = ActorTimerHandles.FindOrAdd(WeakTargetActor);

	FTimerDelegate TimerDelegate;
	TimerDelegate.BindUObject(this, &UCHitStopSubsystem::EndHitStopForActor, TargetActor);

	World->GetTimerManager().SetTimer(ActorTimer, TimerDelegate, Duration, false);

	return true;
}

bool UCHitStopSubsystem::IsActorInHitStop(AActor* TargetActor) const
{
	if (!IsValid(TargetActor))
		return false;

	TWeakObjectPtr<AActor> WeakTargetActor = TargetActor;
	return ActorsInHitStop.Contains(WeakTargetActor);
}

void UCHitStopSubsystem::StartMultipleHitStop(const TArray<AActor*>& TargetActors, float Duration, float TimeScale)
{
	int32 SuccessCount = 0;
	int32 IgnoredCount = 0;

	for (AActor* Actor : TargetActors)
	{
		if (IsValid(Actor))
		{
			if (StartHitStop(Actor, Duration, TimeScale))
				SuccessCount++;
			else
				IgnoredCount++;
		}
	}

	if (bShowDebugMessages && IgnoredCount > 0)
		CLog::Log(FString::Printf(TEXT("[HitStop] 다중 히트 스탑 - 적용: %d, 무시(이미 히트 스탑): %d"), SuccessCount, IgnoredCount));
}

void UCHitStopSubsystem::StartPlayerAndEnemyHitStop(AActor* Player, AActor* HitEnemy,
	const FHitStopParams& PlayerParams, const FHitStopParams& EnemyParams)
{
	if (bShowDebugMessages)
	{
		CLog::Log(TEXT("[HitStop] 플레이어/적 분리 히트 스탑 트리거"));
		CLog::Log(FString::Printf(TEXT("  - 플레이어: Duration=%.3f, TimeScale=%.3f"), PlayerParams.Duration, PlayerParams.TimeScale));
		CLog::Log(FString::Printf(TEXT("  - 적: Duration=%.3f, TimeScale=%.3f"), EnemyParams.Duration, EnemyParams.TimeScale));
	}

	int32 AppliedCount = 0;

	if (IsValid(Player))
	{
		if (StartHitStop(Player, PlayerParams.Duration, PlayerParams.TimeScale))
			AppliedCount++;
	}

	if (IsValid(HitEnemy))
	{
		if (StartHitStop(HitEnemy, EnemyParams.Duration, EnemyParams.TimeScale))
			AppliedCount++;
	}

	if (bShowDebugMessages)
		CLog::Log(FString::Printf(TEXT("[HitStop] 총 %d개 액터에 히트 스탑 적용됨"), AppliedCount));
}

void UCHitStopSubsystem::StartPlayerAndEnemyHitStop(AActor* Player, AActor* HitEnemy, float PlayerDuration,
	float PlayerTimeScale, float EnemyDuration, float EnemyTimeScale)
{
	FHitStopParams PlayerParams(PlayerDuration, PlayerTimeScale);
	FHitStopParams EnemyParams(EnemyDuration, EnemyTimeScale);
	
	StartPlayerAndEnemyHitStop(Player, HitEnemy, PlayerParams, EnemyParams);
}

void UCHitStopSubsystem::EndAllHitStops()
{
	TArray<TWeakObjectPtr<AActor>> ActorsToEnd = ActorsInHitStop.Array();

	for (const TWeakObjectPtr<AActor>& WeakActor : ActorsToEnd)
	{
		if (WeakActor.IsValid())
			EndHitStopForActor(WeakActor.Get());
	}

	if (UWorld* World = GetWorld())
	{
		for (auto& Pair : ActorTimerHandles)
		{
			if (Pair.Value.IsValid())
				World->GetTimerManager().ClearTimer(Pair.Value);
		}
	}

	ActorsInHitStop.Empty();
	OriginalTimeDilations.Empty();
	ActorTimerHandles.Empty();
}

void UCHitStopSubsystem::EndHitStopForActor(AActor* TargetActor)
{
	if (!IsValid(TargetActor))
		return;

	TWeakObjectPtr<AActor> WeakTargetActor = TargetActor;

	ActorsInHitStop.Remove(WeakTargetActor);
	ActorTimerHandles.Remove(WeakTargetActor);

	if (OriginalTimeDilations.Contains(WeakTargetActor))
	{
		TargetActor->CustomTimeDilation = OriginalTimeDilations[WeakTargetActor];
		OriginalTimeDilations.Remove(WeakTargetActor);
	}
	else
	{
		TargetActor->CustomTimeDilation = 1.0f;
	}

	TArray<UActorComponent*> Components;
	TargetActor->GetComponents(Components);

	for (UActorComponent* Component : Components)
	{
		if (!IsValid(Component))
			continue;

		if (UNiagaraComponent* NiagaraComp = Cast<UNiagaraComponent>(Component))
			NiagaraComp->SetFloatParameter(FName("TimeDilation"), 1.0f);
	}

	if (bShowDebugMessages)
	{
		CLog::Log(FString::Printf(TEXT("[HitStop] %s의 히트 스탑 종료"), 
			*TargetActor->GetName()));
	}
}
