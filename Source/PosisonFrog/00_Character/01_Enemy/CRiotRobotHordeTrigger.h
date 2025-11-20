
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "05_System/00_Stage/CEnemySpawnZone.h"
#include "CRiotRobotHordeTrigger.generated.h"

class UBoxComponent;
class ACEnemyCharacterBase;
class ACPlayerCharacter;
class ACStageManager;

/**
 * Horde trigger that spawns a large number of Riot Robots when the player enters a corner of the stage.
 * Enemies are spawned over several batches to avoid CPU and memory spikes.
 */
UCLASS()
class POSISONFROG_API ACRiotRobotHordeTrigger : public AActor
{
        GENERATED_BODY()

public:
        ACRiotRobotHordeTrigger();

protected:
        virtual void BeginPlay() override;
        virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
        UFUNCTION()
        void HandleTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

        UFUNCTION()
        void HandleTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
        
        void StartSpawnSequence();
        void SpawnEnemyBatch();
        void SpawnEnemyFromInfo(const FSpawnTransformInfo& SpawnInfo);
        void CleanupSpawnedEnemies();

        bool BuildSpawnQueue();
        FTransform CreateSpawnTransform(const FVector& BaseLocation) const;
        bool TraceToGround(FVector& Location) const;

private:
        UPROPERTY(EditAnywhere, Category = "Trigger")
        TObjectPtr<UBoxComponent> TriggerBox;

        UPROPERTY(EditAnywhere, Category = "Trigger")
        bool bTriggerOnce = true;

        UPROPERTY(EditAnywhere, Category = "Trigger")
        bool bDebugDrawSpawnArea = false;

        UPROPERTY(EditAnywhere, Category = "Trigger", meta = (ClampMin = "1"))
        int32 StageID = 1;

        UPROPERTY(EditAnywhere, Category = "Spawn|Zone")
        TObjectPtr<ACEnemySpawnZone> LinkedSpawnZone;
        
        UPROPERTY(EditAnywhere, Category = "Spawn|Zone", meta = (EditCondition = "LinkedSpawnZone != nullptr"))
        bool bUseSpawnZoneEnemyTypes = false;
      
        UPROPERTY(EditAnywhere, Category = "Spawn|Zone", meta = (EditCondition = "LinkedSpawnZone != nullptr"))
        bool bShuffleZoneSpawns = true;
       
        UPROPERTY(EditAnywhere, Category = "Spawn|Zone", meta = (EditCondition = "LinkedSpawnZone != nullptr", ClampMin = "0"))
        int32 ZoneSpawnLimit = 0;
        
        UPROPERTY(EditAnywhere, Category = "Spawn")
        TSubclassOf<ACEnemyCharacterBase> RiotRobotClass;

        UPROPERTY(EditAnywhere, Category = "Spawn", meta = (ClampMin = "1"))
        int32 MinSpawnCount = 50;

        UPROPERTY(EditAnywhere, Category = "Spawn", meta = (ClampMin = "1"))
        int32 MaxSpawnCount = 60;

        UPROPERTY(EditAnywhere, Category = "Spawn", meta = (ClampMin = "100"))
        float SpawnRadius = 900.0f;

        UPROPERTY(EditAnywhere, Category = "Spawn")
        float SpawnHeightOffset = 100.0f;

        UPROPERTY(EditAnywhere, Category = "Spawn")
        bool bTraceSpawnToGround = true;

        UPROPERTY(EditAnywhere, Category = "Spawn")
        float TraceHeightAbove = 500.0f;

        UPROPERTY(EditAnywhere, Category = "Spawn")
        float TraceHeightBelow = 800.0f;

        UPROPERTY(EditAnywhere, Category = "Spawn|Batching", meta = (ClampMin = "0.01"))
        float SpawnInterval = 0.12f;

        UPROPERTY(EditAnywhere, Category = "Spawn|Batching", meta = (ClampMin = "1"))
        int32 SpawnPerBatch = 5;

        UPROPERTY(EditAnywhere, Category = "Spawn|Batching", meta = (ClampMin = "0"))
        int32 InitialImmediateSpawn = 5;

        UPROPERTY(EditAnywhere, Category = "Spawn|Batching")
        bool bAutoStartTimer = true;

        bool bPlayerInsideOnBeginPlay = false;
        bool bPlayerExitedAfterBeginPlay = false;
        bool bHasTriggered = false;
        int32 TotalToSpawn = 0;
        int32 SpawnedCount = 0;

        TArray<FSpawnTransformInfo> PendingSpawnInfos;
        TArray<TWeakObjectPtr<ACEnemyCharacterBase>> SpawnedEnemies;

        FTimerHandle SpawnTimerHandle;

        UPROPERTY(EditAnywhere, Category = "StageManager")
        TObjectPtr<ACStageManager> CachedStageManager;
};