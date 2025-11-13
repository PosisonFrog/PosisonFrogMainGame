
#include "CRiotRobotHordeTrigger.h"

#include "Components/BoxComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "00_Character/00_Player/CPlayerCharacter.h"
#include "00_Character/01_Enemy/CEnemyCharacterBase.h"
#include "00_Character/01_Enemy/CRiotRobot.h"

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

void ACRiotRobotHordeTrigger::BeginPlay()
{
        Super::BeginPlay();

        if (TriggerBox)
        {
                TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ACRiotRobotHordeTrigger::HandleTriggerOverlap);
        }

        if (!ensureMsgf(MinSpawnCount <= MaxSpawnCount, TEXT("MinSpawnCount must be <= MaxSpawnCount")))
        {
                MaxSpawnCount = MinSpawnCount;
        }
}

void ACRiotRobotHordeTrigger::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
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

        if (SpawnedCount > 0 || PendingSpawnTransforms.Num() > 0)
        {
                return;
        }

        TotalToSpawn = FMath::RandRange(MinSpawnCount, MaxSpawnCount);
        SpawnedCount = 0;

        BuildSpawnQueue();

        if (PendingSpawnTransforms.Num() == 0)
        {
                UE_LOG(LogTemp, Warning, TEXT("[RiotRobotHordeTrigger] No spawn transforms generated."));
                return;
        }

        if (InitialImmediateSpawn > 0)
        {
                const int32 ImmediateCount = FMath::Min(InitialImmediateSpawn, PendingSpawnTransforms.Num());
                for (int32 Index = 0; Index < ImmediateCount; ++Index)
                {
                        SpawnEnemyAtTransform(PendingSpawnTransforms[Index]);
                }

                PendingSpawnTransforms.RemoveAt(0, ImmediateCount, false);
        }

        if (PendingSpawnTransforms.Num() == 0)
        {
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
        if (PendingSpawnTransforms.Num() == 0)
        {
                GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
                return;
        }

        int32 SpawnThisBatch = 0;
        while (PendingSpawnTransforms.Num() > 0 && SpawnThisBatch < SpawnPerBatch)
        {
                const FTransform SpawnTransform = PendingSpawnTransforms[0];
                PendingSpawnTransforms.RemoveAt(0, 1, false);

                SpawnEnemyAtTransform(SpawnTransform);
                ++SpawnThisBatch;
        }

        if (PendingSpawnTransforms.Num() == 0)
        {
                GetWorldTimerManager().ClearTimer(SpawnTimerHandle);

                if (!bTriggerOnce)
                {
                        bHasTriggered = false;
                }
        }
}

void ACRiotRobotHordeTrigger::SpawnEnemyAtTransform(const FTransform& SpawnTransform)
{
        if (!RiotRobotClass)
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

        ACEnemyCharacterBase* SpawnedEnemy = World->SpawnActor<ACEnemyCharacterBase>(RiotRobotClass, SpawnTransform, SpawnParams);
        if (SpawnedEnemy)
        {
                ++SpawnedCount;
                SpawnedEnemies.RemoveAll([](const TWeakObjectPtr<ACEnemyCharacterBase>& EnemyPtr)
                {
                        return !EnemyPtr.IsValid();
                });
                SpawnedEnemies.Add(SpawnedEnemy);
        }
}

void ACRiotRobotHordeTrigger::BuildSpawnQueue()
{
        PendingSpawnTransforms.Reset();

        const FVector Origin = GetActorLocation();
        const int32 CountToBuild = TotalToSpawn;

        for (int32 Index = 0; Index < CountToBuild; ++Index)
        {
                FTransform SpawnTransform = CreateSpawnTransform(Origin);
                PendingSpawnTransforms.Add(SpawnTransform);
        }

        if (bDebugDrawSpawnArea)
        {
                DrawDebugSphere(GetWorld(), Origin, SpawnRadius, 24, FColor::Red, false, 5.0f, 0, 2.0f);
        }
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