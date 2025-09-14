// Source/PosisonFrogMainGame/Private/Debug/HealOrbDebugStatsSubsystem.cpp
#include "01_Item/HealOrbDebugStatsSubsystem.h"

void UHealOrbDebugStatsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Counts.Reset();
}

void UHealOrbDebugStatsSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UHealOrbDebugStatsSubsystem::Increment(EHealOrbEvent Ev)
{
	switch (Ev)
	{
	case EHealOrbEvent::Spawn:        ++Counts.Spawn; break;
	case EHealOrbEvent::DetectBegin:  ++Counts.DetectBegin; break;
	case EHealOrbEvent::DetectEnd:    ++Counts.DetectEnd; break;
	case EHealOrbEvent::Repath:       ++Counts.Repath; break;
	case EHealOrbEvent::Heal:         ++Counts.Heal; break;
	case EHealOrbEvent::Expire:       ++Counts.Expire; break;
	case EHealOrbEvent::PoolAcquire:  ++Counts.PoolAcquire; break;
	case EHealOrbEvent::PoolRelease:  ++Counts.PoolRelease; break;
	default: break;
	}
}
