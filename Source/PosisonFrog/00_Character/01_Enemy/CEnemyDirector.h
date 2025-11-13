#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "05_System/00_Stage/CEnemySpawnZone.h"
#include "CEnemyDirector.generated.h"

class ACEnemyCharacterBase;
class ACStageManager;
class UCBaseHealthComponent;
struct FSpawnTransformInfo;

USTRUCT(BlueprintType)
struct FEnemyTypeLimit
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category="Budget")
    FName TypeTag = NAME_None;

    UPROPERTY(EditAnywhere, Category="Budget", meta=(ClampMin="0"))
    int32 MaxActive = 0;
};

USTRUCT()
struct FQueuedEnemySpawn
{
    GENERATED_BODY()

    UPROPERTY()
    TWeakObjectPtr<ACStageManager> Requester;

    UPROPERTY()
    int32 StageID = INDEX_NONE;

    UPROPERTY()
    FSpawnTransformInfo SpawnInfo;

    UPROPERTY()
    FName OverrideTypeTag = NAME_None;

    UPROPERTY()
    TWeakObjectPtr<ACEnemyCharacterBase> ExistingEnemy;

    UPROPERTY()
    bool bUseExistingActor = false;
};

UCLASS()
class POSISONFROG_API ACEnemyDirector : public AActor
{
    GENERATED_BODY()

public:
    ACEnemyDirector();

    virtual void BeginPlay() override;

#if WITH_EDITOR
    virtual void OnConstruction(const FTransform& Transform) override;
#endif

    bool RequestSpawn(ACStageManager* Requester, int32 StageID, const FSpawnTransformInfo& SpawnInfo, FName OverrideTypeTag = NAME_None, bool bForce = false);

    bool RequestExistingActivation(ACStageManager* Requester, int32 StageID, ACEnemyCharacterBase* ExistingEnemy, FName OverrideTypeTag = NAME_None, bool bForce = false);

    void NotifyEnemyDestroyed(ACEnemyCharacterBase* Enemy);

    void ForceReleaseEnemy(ACEnemyCharacterBase* Enemy);

    void CancelRequestsForStage(ACStageManager* Requester, int32 StageID);

protected:
    bool CanSpawn(FName TypeTag, bool bForce) const;
    bool HasGlobalSlot() const;
    FName ResolveTypeTag(TSubclassOf<ACEnemyCharacterBase> EnemyClass, FName OverrideTypeTag) const;

    ACEnemyCharacterBase* SpawnEnemyActor(const FSpawnTransformInfo& SpawnInfo) const;
    void RegisterActiveEnemy(ACEnemyCharacterBase* Enemy, FName TypeTag);
    void UnregisterActiveEnemy(ACEnemyCharacterBase* Enemy);

    void ProcessSpawnQueue();
    void RebuildTypeLimitLookup();

    void RemoveQueuedEntriesForEnemy(ACEnemyCharacterBase* Enemy);

    UFUNCTION()
    void HandleEnemyDeath(AActor* DeadActor);

private:
    UPROPERTY(EditAnywhere, Category="Budget", meta=(ClampMin="0"))
    int32 GlobalMaxActiveEnemies = 18;

    UPROPERTY(EditAnywhere, Category="Budget")
    TArray<FEnemyTypeLimit> TypeLimits;

    UPROPERTY(EditAnywhere, Category="Debug")
    bool bEnableDebugLogs = false;

    UPROPERTY(VisibleAnywhere, Category="Runtime")
    int32 ActiveEnemyCount = 0;

    UPROPERTY(VisibleAnywhere, Category="Runtime")
    TMap<FName, int32> ActiveTypeCounts;

    UPROPERTY()
    TSet<TWeakObjectPtr<ACEnemyCharacterBase>> ActiveEnemies;

    UPROPERTY()
    TMap<TWeakObjectPtr<ACEnemyCharacterBase>, FName> EnemyTypeCache;

    UPROPERTY()
    TArray<FQueuedEnemySpawn> SpawnQueue;

    TMap<FName, int32> TypeLimitLookup;
};
