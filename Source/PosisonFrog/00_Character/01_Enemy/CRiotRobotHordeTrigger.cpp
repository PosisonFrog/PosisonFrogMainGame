
#include "CRiotRobotHordeTrigger.h"

#include "Components/BoxComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "00_Character/00_Player/CPlayerCharacter.h"
#include "00_Character/01_Enemy/CEnemyCharacterBase.h"
#include "00_Character/01_Enemy/CRiotRobot.h"
#include "05_System/00_Stage/CEnemySpawnZone.h"
#include "05_System/00_Stage/CStageManager.h"
#include "99_Util/CLog.h"

namespace
{
        constexpr float DefaultSpawnYawJitter = 180.0f;
}

ACRiotRobotHordeTrigger::ACRiotRobotHordeTrigger()
{
        PrimaryActorTick.bCanEverTick = false;

        RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

        TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
        TriggerBox->SetupAttachment(RootComponent);
        TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
        TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
        TriggerBox->SetGenerateOverlapEvents(true);
        TriggerBox->SetBoxExtent(FVector(200.f));

        RiotRobotClass = ACRiotRobot::StaticClass();
}

void ACRiotRobotHordeTrigger::DeactivateTrigger()
{
        if (TriggerBox)
        {
                TriggerBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }

        if (SpawnTimerHandle.IsValid())
        {
                if (UWorld* World = GetWorld())
                {
                        World->GetTimerManager().ClearTimer(SpawnTimerHandle);
                }
        }

        PendingSpawnInfos.Empty();
        bHasTriggered = true;

        CLog::Log(FString::Printf(TEXT("[CRiotRobotHordeTrigger::DeactivateTrigger] Trigger deactivated: %s"), *GetName()));
}

void ACRiotRobotHordeTrigger::BeginPlay()
{
        Super::BeginPlay();

        if (TriggerBox)
        {
                TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ACRiotRobotHordeTrigger::HandleTriggerOverlap);
                TriggerBox->OnComponentEndOverlap.AddDynamic(this, &ACRiotRobotHordeTrigger::HandleTriggerEndOverlap);
              
                if (UWorld* World = GetWorld())
                {
                        if (ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(World, 0))
                        {
                                if (TriggerBox->IsOverlappingActor(PlayerCharacter))
                                {
                                        bPlayerInsideOnBeginPlay = true;
                                        bPlayerExitedAfterBeginPlay = false;
                                }
                        }
                }
        }
 
        if (!ensureMsgf(MinSpawnCount <= MaxSpawnCount, TEXT("MinSpawnCount must be <= MaxSpawnCount")))
        {
                MaxSpawnCount = MinSpawnCount;
        }
}

void ACRiotRobotHordeTrigger::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
        if (TriggerBox)
        {
                TriggerBox->OnComponentBeginOverlap.RemoveDynamic(this, &ACRiotRobotHordeTrigger::HandleTriggerOverlap);
                TriggerBox->OnComponentEndOverlap.RemoveDynamic(this, &ACRiotRobotHordeTrigger::HandleTriggerEndOverlap);
        }

        if (SpawnTimerHandle.IsValid())
        {
                if (UWorld* World = GetWorld())
                {
                        World->GetTimerManager().ClearTimer(SpawnTimerHandle);
                }
        }
        CleanupSpawnedEnemies();

        Super::EndPlay(EndPlayReason);
}

void ACRiotRobotHordeTrigger::HandleTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
        if (OtherActor && OtherActor->IsA(ACPlayerCharacter::StaticClass()))
        {
                if (bPlayerInsideOnBeginPlay && !bPlayerExitedAfterBeginPlay)
                {
                        return;
                }
                        
                bPlayerInsideOnBeginPlay = false;
                bPlayerExitedAfterBeginPlay = false;
        }

        if (bHasTriggered && bTriggerOnce)
        {
                return;
        }

        if (!OtherActor)
        {
                return;
        }

        if (!OtherActor->IsA(ACPlayerCharacter::StaticClass()))
        {
                return;
        }

        bHasTriggered = true;

        if (bTriggerOnce && TriggerBox)
        {
                TriggerBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }

        StartSpawnSequence();
}

void ACRiotRobotHordeTrigger::StartSpawnSequence()
{
        if (!RiotRobotClass)
        {
                UE_LOG(LogTemp, Warning, TEXT("[RiotRobotHordeTrigger] RiotRobotClass is null"));
                return;
        }

        SpawnedEnemies.RemoveAll([](const TWeakObjectPtr<ACEnemyCharacterBase>& EnemyPtr)
        {
                return !EnemyPtr.IsValid();
        });

        if (!bTriggerOnce && SpawnedEnemies.Num() > 0)
        {
                // Horde still active, skip spawning again until they are cleared.
                return;
        }

        if (SpawnedCount > 0 || PendingSpawnInfos.Num() > 0)
        {
                return;
        }

        const bool bUsingSpawnZone = IsValid(LinkedSpawnZone);
       
        if (!bUsingSpawnZone)
        {
                TotalToSpawn = FMath::RandRange(MinSpawnCount, MaxSpawnCount);
        }
        SpawnedCount = 0;

        if (!BuildSpawnQueue())
        {
                UE_LOG(LogTemp, Warning, TEXT("[RiotRobotHordeTrigger] Failed to build spawn queue."));
                return;
        }

        if (PendingSpawnInfos.Num() == 0)
        {
                UE_LOG(LogTemp, Warning, TEXT("[RiotRobotHordeTrigger] No spawn transforms generated."));
                return;
        }

        if (InitialImmediateSpawn > 0)
        {
                const int32 ImmediateCount = FMath::Min(InitialImmediateSpawn, PendingSpawnInfos.Num());
                for (int32 Index = 0; Index < ImmediateCount; ++Index)
                {
                        SpawnEnemyFromInfo(PendingSpawnInfos[Index]);
                }

                PendingSpawnInfos.RemoveAt(0, ImmediateCount, false);
        }

        if (PendingSpawnInfos.Num() == 0)        {
                if (!bTriggerOnce)
                {
                        bHasTriggered = false;
                }
                return;
        }

        if (bAutoStartTimer)
        {
                GetWorldTimerManager().SetTimer(SpawnTimerHandle, this, &ACRiotRobotHordeTrigger::SpawnEnemyBatch, SpawnInterval, true);
        }
}

void ACRiotRobotHordeTrigger::SpawnEnemyBatch()
{
        if (PendingSpawnInfos.Num() == 0)
        {
                GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
                return;
        }

        int32 SpawnThisBatch = 0;
        while (PendingSpawnInfos.Num() > 0 && SpawnThisBatch < SpawnPerBatch)
        {
                const FSpawnTransformInfo SpawnInfo = PendingSpawnInfos[0];
                PendingSpawnInfos.RemoveAt(0, 1, false);

 SpawnEnemyFromInfo(SpawnInfo);
                ++SpawnThisBatch;
        }

        if (PendingSpawnInfos.Num() == 0)
        {
                GetWorldTimerManager().ClearTimer(SpawnTimerHandle);

                if (!bTriggerOnce)
                {
                        bHasTriggered = false;
                }
        }
}

void ACRiotRobotHordeTrigger::SpawnEnemyFromInfo(const FSpawnTransformInfo& SpawnInfo)
{
        TSubclassOf<ACEnemyCharacterBase> EnemyClass = SpawnInfo.EnemyClass;
        if (!EnemyClass)
        {
                EnemyClass = RiotRobotClass;
        }

        if (!EnemyClass)
        {
                return;
        }

        UWorld* World = GetWorld();
        if (!World)
        {
                return;
        }

        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

        ACEnemyCharacterBase* SpawnedEnemy = World->SpawnActor<ACEnemyCharacterBase>(EnemyClass, SpawnInfo.Transform, SpawnParams);
        if (SpawnedEnemy)
        {
                ++SpawnedCount;
                SpawnedEnemies.RemoveAll([](const TWeakObjectPtr<ACEnemyCharacterBase>& EnemyPtr)
                {
                        return !EnemyPtr.IsValid();
                });
                SpawnedEnemies.Add(SpawnedEnemy);

                if (CachedStageManager)
                {
                        CachedStageManager->RegisterHordeEnemy(SpawnedEnemy, StageID);
                }
        }
}

bool ACRiotRobotHordeTrigger::BuildSpawnQueue()
{
        PendingSpawnInfos.Reset();
        
        const FVector Origin = GetActorLocation();
        if (IsValid(LinkedSpawnZone))
        {
                TArray<FSpawnTransformInfo> ZoneSpawns = LinkedSpawnZone->GenerateSpawnTransforms();
            
                if (ZoneSpawns.Num() == 0)
                {
                        return false;
                }
                if (bShuffleZoneSpawns)
                {
                        for (int32 Index = 0; Index < ZoneSpawns.Num() - 1; ++Index)
                        {
                                const int32 SwapIndex = FMath::RandRange(Index, ZoneSpawns.Num() - 1);
                                if (SwapIndex != Index)
                                {
                                        ZoneSpawns.Swap(Index, SwapIndex);
                                }
                        }
                }
          
                if (ZoneSpawnLimit > 0 && ZoneSpawns.Num() > ZoneSpawnLimit)
                {
                        ZoneSpawns.SetNum(ZoneSpawnLimit);
                }
             
                for (FSpawnTransformInfo& SpawnInfo : ZoneSpawns)
                {
                        if (!bUseSpawnZoneEnemyTypes || !SpawnInfo.EnemyClass)
                        {
                                SpawnInfo.EnemyClass = RiotRobotClass;
                        }
                }
            
                TotalToSpawn = ZoneSpawns.Num();
                PendingSpawnInfos = MoveTemp(ZoneSpawns);
        }
        else
        {
                const int32 CountToBuild = TotalToSpawn;
             
                for (int32 Index = 0; Index < CountToBuild; ++Index)
                {
                        FTransform SpawnTransform = CreateSpawnTransform(Origin);
                        PendingSpawnInfos.Add(FSpawnTransformInfo(SpawnTransform, RiotRobotClass));
                }
               
                if (bDebugDrawSpawnArea)
                {
                        DrawDebugSphere(GetWorld(), Origin, SpawnRadius, 24, FColor::Red, false, 5.0f, 0, 2.0f);
                }
        }
        return PendingSpawnInfos.Num() > 0;
}

FTransform ACRiotRobotHordeTrigger::CreateSpawnTransform(const FVector& BaseLocation) const
{
        const FVector2D RandomPoint2D = FMath::RandPointInCircle(SpawnRadius);
        FVector SpawnLocation = BaseLocation + FVector(RandomPoint2D, 0.0f);

        if (bTraceSpawnToGround)
        {
                TraceToGround(SpawnLocation);
        }
        else
        {
                SpawnLocation.Z += SpawnHeightOffset;
        }

        FRotator SpawnRotation = GetActorRotation();
        SpawnRotation.Yaw += FMath::FRandRange(-DefaultSpawnYawJitter, DefaultSpawnYawJitter);

        return FTransform(SpawnRotation, SpawnLocation);
}

bool ACRiotRobotHordeTrigger::TraceToGround(FVector& Location) const
{
        UWorld* World = GetWorld();
        if (!World)
        {
                return false;
        }

        const FVector TraceStart = Location + FVector(0.f, 0.f, TraceHeightAbove);
        const FVector TraceEnd = Location - FVector(0.f, 0.f, TraceHeightBelow);

        FHitResult Hit;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(RiotRobotHordeTrace), false, this);
        if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, Params))
        {
                Location = Hit.Location + FVector(0.f, 0.f, SpawnHeightOffset);
                return true;
        }

        Location.Z += SpawnHeightOffset;
        return false;
}

void ACRiotRobotHordeTrigger::CleanupSpawnedEnemies()
{
        for (TWeakObjectPtr<ACEnemyCharacterBase>& EnemyPtr : SpawnedEnemies)
        {
                if (ACEnemyCharacterBase* Enemy = EnemyPtr.Get())
                {
                        if (!Enemy->IsPendingKill())
                        {
                                Enemy->Destroy();
                        }
                }
        }

        SpawnedEnemies.Empty();
}

void ACRiotRobotHordeTrigger::HandleTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
        if (!OtherActor)
        {
                return;
        }
       
        if (OtherActor->IsA(ACPlayerCharacter::StaticClass()) && bPlayerInsideOnBeginPlay)
        {
                bPlayerExitedAfterBeginPlay = true;
        }
}