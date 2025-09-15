// Fill out your copyright notice in the Description page of Project Settings.


#include "CHealOrbPoolSubsystem.h"
#include "01_Item/CHealOrb.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UCHealOrbPoolSubsystem::Prewarm(UWorld* World, int32 Count)
{
	if (!World || !*OrbClass) return;

	for (int32 i = 0; i < Count; ++i)
	{
		ACHealOrb* Orb = World->SpawnActorDeferred<ACHealOrb>(OrbClass, FTransform::Identity);
		if (!Orb) continue;

		// 풀링용 플래그
		Orb->bUsePooling = true; // ACHealOrb에 공개 플래그 존재
		Orb->FinishSpawning(FTransform::Identity);
		Orb->ReleaseOrb(true);   // 비활성화 후 풀에 반환
		InactivePool.Add(Orb);
	}
	BroadcastCounters();
}

ACHealOrb* UCHealOrbPoolSubsystem::Acquire(UWorld* World, const FTransform& Xform, AActor* PreferredTarget)
{
	if (!World || !*OrbClass) return nullptr;

	ACHealOrb* Orb = nullptr;

	if (InactivePool.Num() > 0)
	{
		Orb = InactivePool.Pop(false);
	}
	else
	{
		Orb = World->SpawnActor<ACHealOrb>(OrbClass, Xform);
		if (!Orb) return nullptr;
		Orb->bUsePooling = true; // 풀 관리 대상
	}

	// 활성 전환
	Orb->SetActorTransform(Xform);
	Orb->ActivateOrb(PreferredTarget);

	ActivePool.Add(Orb);
	BroadcastCounters();
	return Orb;
}

void UCHealOrbPoolSubsystem::Release(ACHealOrb* Orb)
{
	if (!Orb) return;
	if (!IsValid(Orb)) return; // 이미 파괴된 경우 방어

	Orb->ReleaseOrb(true);

	ActivePool.Remove(Orb);
	InactivePool.Add(Orb);
	BroadcastCounters();
}

void UCHealOrbPoolSubsystem::NotifyPicked(ACHealOrb* Orb)
{
	++TotalPicked;
	// 픽업 즉시 풀 반환
	Release(Orb);
}

void UCHealOrbPoolSubsystem::BroadcastCounters()
{
	OnCountersChanged.Broadcast(ActivePool.Num(), TotalPicked);
}
