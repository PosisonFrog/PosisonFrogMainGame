
#include "CEnemyDirector.h"

#include "05_System/00_Stage/CEnemySpawnZone.h"
#include "00_Character/01_Enemy/CEnemyCharacterBase.h"
#include "00_Character/02_Component/CBaseHealthComponent.h"
#include "99_Util/CLog.h"
#include "AIController.h"
#include "05_System/00_Stage/CStageManager.h"

ACEnemyDirector::ACEnemyDirector()
{
    PrimaryActorTick.bCanEverTick = false;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ACEnemyDirector::BeginPlay()
{
    Super::BeginPlay();

    RebuildTypeLimitLookup();
}

#if WITH_EDITOR
void ACEnemyDirector::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    RebuildTypeLimitLookup();
}
#endif

bool ACEnemyDirector::RequestSpawn(ACStageManager* Requester, int32 StageID, const FSpawnTransformInfo& SpawnInfo, FName OverrideTypeTag, bool bForce)
{
    if (!SpawnInfo.EnemyClass)
    {
        return false;
    }

    FName TypeTag = ResolveTypeTag(SpawnInfo.EnemyClass, OverrideTypeTag);

    if (!CanSpawn(TypeTag, bForce))
    {
        FQueuedEnemySpawn& QueueEntry = SpawnQueue.Emplace_GetRef();
        QueueEntry.Requester = Requester;
        QueueEntry.StageID = StageID;
        QueueEntry.SpawnInfo = SpawnInfo;
        QueueEntry.OverrideTypeTag = OverrideTypeTag;
        QueueEntry.bUseExistingActor = false;

        if (bEnableDebugLogs)
        {
            CLog::Log(FString::Printf(TEXT("[EnemyDirector] Queue spawn (Stage %d) - Type: %s"), StageID, *TypeTag.ToString()));
        }
        return false;
    }

    ACEnemyCharacterBase* SpawnedEnemy = SpawnEnemyActor(SpawnInfo);

    if (!SpawnedEnemy)
    {
        return false;
    }

    RegisterActiveEnemy(SpawnedEnemy, TypeTag);

    if (Requester)
    {
        Requester->HandleEnemySpawnedFromDirector(SpawnedEnemy, StageID, false);
    }

    return true;
}

bool ACEnemyDirector::RequestExistingActivation(ACStageManager* Requester, int32 StageID, ACEnemyCharacterBase* ExistingEnemy, FName OverrideTypeTag, bool bForce)
{
    if (!IsValid(ExistingEnemy))
    {
        return false;
    }

    FName TypeTag = OverrideTypeTag.IsNone() ? ExistingEnemy->GetEnemyTypeTag() : OverrideTypeTag;

    if (!CanSpawn(TypeTag, bForce))
    {
        FQueuedEnemySpawn& QueueEntry = SpawnQueue.Emplace_GetRef();
        QueueEntry.Requester = Requester;
        QueueEntry.StageID = StageID;
        QueueEntry.OverrideTypeTag = OverrideTypeTag.IsNone() ? TypeTag : OverrideTypeTag;
        QueueEntry.ExistingEnemy = ExistingEnemy;
        QueueEntry.bUseExistingActor = true;

        if (bEnableDebugLogs)
        {
            CLog::Log(FString::Printf(TEXT("[EnemyDirector] Queue activation (Stage %d) - Type: %s"), StageID, *TypeTag.ToString()));
        }

        return false;
    }

    RegisterActiveEnemy(ExistingEnemy, TypeTag);

    if (Requester)
    {
        Requester->HandleEnemySpawnedFromDirector(ExistingEnemy, StageID, true);
    }

    return true;
}

void ACEnemyDirector::NotifyEnemyDestroyed(ACEnemyCharacterBase* Enemy)
{
    if (!Enemy)
    {
        return;
    }

    UnregisterActiveEnemy(Enemy);
    ProcessSpawnQueue();
}

void ACEnemyDirector::ForceReleaseEnemy(ACEnemyCharacterBase* Enemy)
{
    if (!Enemy)
    {
        return;
    }

    RemoveQueuedEntriesForEnemy(Enemy);
    UnregisterActiveEnemy(Enemy);
}

void ACEnemyDirector::CancelRequestsForStage(ACStageManager* Requester, int32 StageID)
{
    SpawnQueue.RemoveAll([Requester, StageID](const FQueuedEnemySpawn& Entry)
    {
        const bool bMatchesRequester = !Requester || Entry.Requester.Get() == Requester;
        const bool bMatchesStage = StageID == INDEX_NONE || Entry.StageID == StageID;
        return bMatchesRequester && bMatchesStage;
    });
}

bool ACEnemyDirector::CanSpawn(FName TypeTag, bool bForce) const
{
    if (bForce)
    {
        return true;
    }

    if (!HasGlobalSlot())
    {
        return false;
    }

    if (!TypeTag.IsNone())
    {
        if (const int32* TypeLimit = TypeLimitLookup.Find(TypeTag))
        {
            if (*TypeLimit > 0)
            {
                const int32 CurrentCount = ActiveTypeCounts.FindRef(TypeTag);
                return CurrentCount < *TypeLimit;
            }
        }
    }

    return true;
}

bool ACEnemyDirector::HasGlobalSlot() const
{
    return GlobalMaxActiveEnemies <= 0 || ActiveEnemyCount < GlobalMaxActiveEnemies;
}

FName ACEnemyDirector::ResolveTypeTag(TSubclassOf<ACEnemyCharacterBase> EnemyClass, FName OverrideTypeTag) const
{
    if (!OverrideTypeTag.IsNone())
    {
        return OverrideTypeTag;
    }

    if (!EnemyClass)
    {
        return NAME_None;
    }

    if (const ACEnemyCharacterBase* DefaultEnemy = EnemyClass->GetDefaultObject<ACEnemyCharacterBase>())
    {
        return DefaultEnemy->GetEnemyTypeTag();
    }

    return NAME_None;
}

ACEnemyCharacterBase* ACEnemyDirector::SpawnEnemyActor(const FSpawnTransformInfo& SpawnInfo) const
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    ACEnemyCharacterBase* SpawnedEnemy = World->SpawnActor<ACEnemyCharacterBase>(SpawnInfo.EnemyClass, SpawnInfo.Transform, SpawnParams);

    if (SpawnedEnemy)
    {
        SpawnedEnemy->SaveInitialTransform();
    }

    return SpawnedEnemy;
}

void ACEnemyDirector::RegisterActiveEnemy(ACEnemyCharacterBase* Enemy, FName TypeTag)
{
    if (!IsValid(Enemy))
    {
        return;
    }

    if (ActiveEnemies.Contains(Enemy))
    {
        return;
    }

    ActiveEnemies.Add(Enemy);
    EnemyTypeCache.Add(Enemy, TypeTag);

    ActiveEnemyCount = ActiveEnemies.Num();

    if (!TypeTag.IsNone())
    {
        int32& Count = ActiveTypeCounts.FindOrAdd(TypeTag);
        Count++;
    }

    if (UCBaseHealthComponent* HealthComp = Enemy->FindComponentByClass<UCBaseHealthComponent>())
    {
       // 에러나서 주석처리. if (!HealthComp->OnDeath.IsBoundToObject(this))
       // AddDynamic에서 AddUniqueDynamic으로 변경.
            HealthComp->OnDeath.AddUniqueDynamic(this, &ACEnemyDirector::HandleEnemyDeath);
        
    }

    if (bEnableDebugLogs)
    {
        FString TypeString = TypeTag.IsNone() ? TEXT("None") : TypeTag.ToString();
        CLog::Log(FString::Printf(TEXT("[EnemyDirector] Activate %s (Type: %s) Active:%d"), *Enemy->GetName(), *TypeString, ActiveEnemyCount));
    }
}

void ACEnemyDirector::UnregisterActiveEnemy(ACEnemyCharacterBase* Enemy)
{
    if (!ActiveEnemies.Contains(Enemy))
    {
        return;
    }

    ActiveEnemies.Remove(Enemy);
    ActiveEnemyCount = ActiveEnemies.Num();

    FName* FoundType = EnemyTypeCache.Find(Enemy);
    if (FoundType)
    {
        if (!FoundType->IsNone())
        {
            if (int32* Count = ActiveTypeCounts.Find(*FoundType))
            {
                *Count = FMath::Max(0, *Count - 1);
            }
        }
        EnemyTypeCache.Remove(Enemy);
    }

    if (UCBaseHealthComponent* HealthComp = Enemy->FindComponentByClass<UCBaseHealthComponent>())
    {
        HealthComp->OnDeath.RemoveDynamic(this, &ACEnemyDirector::HandleEnemyDeath);
    }

    if (bEnableDebugLogs)
    {
        CLog::Log(FString::Printf(TEXT("[EnemyDirector] Release %s Active:%d"), *Enemy->GetName(), ActiveEnemyCount));
    }
}

void ACEnemyDirector::ProcessSpawnQueue()
{
    if (SpawnQueue.Num() == 0)
    {
        return;
    }

    int32 IterationCount = SpawnQueue.Num();

    for (int32 Index = 0; Index < IterationCount; ++Index)
    {
        if (SpawnQueue.Num() == 0)
        {
            break;
        }

        FQueuedEnemySpawn Entry = SpawnQueue[0];
        SpawnQueue.RemoveAt(0);

        ACStageManager* Requester = Entry.Requester.Get();

        if (!Requester)
        {
            continue;
        }

        ACEnemyCharacterBase* TargetEnemy = Entry.bUseExistingActor ? Entry.ExistingEnemy.Get() : nullptr;

        if (Entry.bUseExistingActor && !IsValid(TargetEnemy))
        {
            continue;
        }

        FName TypeTag = Entry.bUseExistingActor
            ? (Entry.OverrideTypeTag.IsNone() && TargetEnemy ? TargetEnemy->GetEnemyTypeTag() : Entry.OverrideTypeTag)
            : ResolveTypeTag(Entry.SpawnInfo.EnemyClass, Entry.OverrideTypeTag);

        if (!CanSpawn(TypeTag, false))
        {
            // 재대기열 - 앞으로 돌려보내 무한 루프 방지
            SpawnQueue.Add(Entry);
            continue;
        }

        if (Entry.bUseExistingActor)
        {
            RegisterActiveEnemy(TargetEnemy, TypeTag);
            Requester->HandleEnemySpawnedFromDirector(TargetEnemy, Entry.StageID, true);
        }
        else
        {
            ACEnemyCharacterBase* SpawnedEnemy = SpawnEnemyActor(Entry.SpawnInfo);
            if (!SpawnedEnemy)
            {
                continue;
            }

            RegisterActiveEnemy(SpawnedEnemy, TypeTag);
            Requester->HandleEnemySpawnedFromDirector(SpawnedEnemy, Entry.StageID, false);
        }
    }
}

void ACEnemyDirector::RebuildTypeLimitLookup()
{
    TypeLimitLookup.Empty();

    for (const FEnemyTypeLimit& Limit : TypeLimits)
    {
        if (!Limit.TypeTag.IsNone())
        {
            TypeLimitLookup.Add(Limit.TypeTag, Limit.MaxActive);
        }
    }
}

void ACEnemyDirector::RemoveQueuedEntriesForEnemy(ACEnemyCharacterBase* Enemy)
{
    SpawnQueue.RemoveAll([Enemy](const FQueuedEnemySpawn& Entry)
    {
        return Entry.bUseExistingActor && Entry.ExistingEnemy == Enemy;
    });
}

void ACEnemyDirector::HandleEnemyDeath(AActor* DeadActor)
{
    ACEnemyCharacterBase* Enemy = Cast<ACEnemyCharacterBase>(DeadActor);
    if (!Enemy)
    {
        return;
    }

    NotifyEnemyDestroyed(Enemy);
}
