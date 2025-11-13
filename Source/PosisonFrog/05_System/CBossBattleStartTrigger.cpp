#include "CBossBattleStartTrigger.h"
#include "Components/BoxComponent.h"
#include "00_Character/01_Enemy/CEnemyBossCharacter.h"
#include "00_Character/00_Player/CPlayerCharacter.h"
#include "00_Stage/CStageManager.h"
#include "Kismet/GameplayStatics.h"

ACBossBattleStartTrigger::ACBossBattleStartTrigger()
{
    PrimaryActorTick.bCanEverTick = false;

    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    RootComponent = TriggerBox;

    TriggerBox->SetBoxExtent(FVector(200.0f, 200.0f, 200.0f));
    TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    TriggerBox->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
    TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    TriggerBox->SetGenerateOverlapEvents(true);

#if WITH_EDITORONLY_DATA
    TriggerBox->bHiddenInGame = false;
    TriggerBox->SetVisibility(true);
#endif
}

void ACBossBattleStartTrigger::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Error, TEXT("================================================================="));
    UE_LOG(LogTemp, Error, TEXT("[BossTrigger] BeginPlay"));
    UE_LOG(LogTemp, Error, TEXT("[BossTrigger] 액터 이름: %s"), *GetName());
    UE_LOG(LogTemp, Error, TEXT("[BossTrigger] bIsEnabled: %s"), bIsEnabled ? TEXT("TRUE") : TEXT("FALSE"));
    UE_LOG(LogTemp, Error, TEXT("[BossTrigger] bTriggerOnce: %s"), bTriggerOnce ? TEXT("TRUE") : TEXT("FALSE"));
    UE_LOG(LogTemp, Error, TEXT("[BossTrigger] bSkipIntro: %s"), bSkipIntro ? TEXT("TRUE") : TEXT("FALSE"));
    UE_LOG(LogTemp, Error, TEXT("[BossTrigger] TargetBoss: %s"), *GetNameSafe(TargetBoss));
    UE_LOG(LogTemp, Error, TEXT("================================================================="));

    if (IsValid(TriggerBox))
    {
        TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ACBossBattleStartTrigger::OnTriggerBeginOverlap);
    }

    if (!IsValid(TargetBoss))
    {
        UE_LOG(LogTemp, Warning, TEXT("[BossTrigger] TargetBoss가 설정되지 않음"));
    }
}

void ACBossBattleStartTrigger::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (IsValid(TriggerBox))
    {
        TriggerBox->OnComponentBeginOverlap.RemoveDynamic(this, &ACBossBattleStartTrigger::OnTriggerBeginOverlap);
    }

    Super::EndPlay(EndPlayReason);
}

void ACBossBattleStartTrigger::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // 트리거 비활성화 상태면 무시
    if (!bIsEnabled)
    {
        return;
    }

    // 이미 트리거되었고 한 번만 작동하는 설정이면 무시
    if (bHasTriggered && bTriggerOnce)
    {
        return;
    }

    // 플레이어 캐릭터인지 확인
    ACPlayerCharacter* PlayerCharacter = Cast<ACPlayerCharacter>(OtherActor);
    if (!IsValid(PlayerCharacter))
    {
        return;
    }
    
    UE_LOG(LogTemp, Error, TEXT("================================================================="));
    UE_LOG(LogTemp, Error, TEXT("[BossTrigger] 플레이어 감지"));
    UE_LOG(LogTemp, Error, TEXT("[BossTrigger] Player: %s"), *GetNameSafe(PlayerCharacter));
    UE_LOG(LogTemp, Error, TEXT("[BossTrigger] Boss: %s"), *GetNameSafe(TargetBoss));

    // 보스 레퍼런스 유효성 확인
    if (!IsValid(TargetBoss))
    {
        UE_LOG(LogTemp, Error, TEXT("[BossTrigger] TargetBoss가 유효하지 않음"));
        UE_LOG(LogTemp, Error, TEXT("================================================================="));
        return;
    }

    // StageManager에 보스 전투 시작 알림
    UE_LOG(LogTemp, Error, TEXT("[BossTrigger] StageManager 검색"));
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACStageManager::StaticClass(), FoundActors);
    
    if (FoundActors.Num() > 0)
    {
        ACStageManager* StageManager = Cast<ACStageManager>(FoundActors[0]);
        if (IsValid(StageManager))
        {
            UE_LOG(LogTemp, Error, TEXT("[BossTrigger] StageManager 발견: %s"), *StageManager->GetName());
            UE_LOG(LogTemp, Error, TEXT("[BossTrigger] StageManager에 보스 전투 시작 알림 전송"));
            StageManager->OnBossBattleStartRequested();
            UE_LOG(LogTemp, Error, TEXT("[BossTrigger] 알림 전송 완료"));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[BossTrigger] StageManager 캐스팅 실패"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[BossTrigger] StageManager를 찾을 수 없음 - 배리어 제어 건너뜀"));
    }

    // 보스 전투 시작
    UE_LOG(LogTemp, Error, TEXT("[BossTrigger] 보스 전투 시작"));
    TargetBoss->StartBossBattle(bSkipIntro);
    bHasTriggered = true;
    UE_LOG(LogTemp, Error, TEXT("================================================================="));

    // 한 번만 작동하는 경우 트리거 비활성화
    if (bTriggerOnce)
    {
        bIsEnabled = false;
        if (IsValid(TriggerBox))
        {
            TriggerBox->SetGenerateOverlapEvents(false);
        }
    }
}

void ACBossBattleStartTrigger::ManualTrigger()
{
    UE_LOG(LogTemp, Warning, TEXT("[BossTrigger] ManualTrigger 호출됨"));
    
    if (!IsValid(TargetBoss))
    {
        UE_LOG(LogTemp, Error, TEXT("[BossTrigger] ManualTrigger 실패 - TargetBoss가 nullptr"));
        return;
    }
    
    if (bHasTriggered && bTriggerOnce)
    {
        UE_LOG(LogTemp, Warning, TEXT("[BossTrigger] ManualTrigger 무시 - 이미 트리거됨"));
        return;
    }

    // StageManager에 알림
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACStageManager::StaticClass(), FoundActors);
    
    if (FoundActors.Num() > 0)
    {
        ACStageManager* StageManager = Cast<ACStageManager>(FoundActors[0]);
        if (IsValid(StageManager))
        {
            UE_LOG(LogTemp, Error, TEXT("[BossTrigger] Manual: StageManager에 알림 전송"));
            StageManager->OnBossBattleStartRequested();
        }
    }
    
    UE_LOG(LogTemp, Error, TEXT("[BossTrigger] Manual 보스 전투 시작"));
    TargetBoss->StartBossBattle(bSkipIntro);
    bHasTriggered = true;
    
    if (bTriggerOnce)
    {
        bIsEnabled = false;
    }
}

void ACBossBattleStartTrigger::SetTriggerEnabled(bool bEnabled)
{
    bIsEnabled = bEnabled;
    UE_LOG(LogTemp, Log, TEXT("[BossTrigger] 트리거 %s"), bEnabled ? TEXT("활성화") : TEXT("비활성화"));
    
    if (!bEnabled && IsValid(TriggerBox))
    {
        TriggerBox->SetGenerateOverlapEvents(false);
    }
    else if (bEnabled && IsValid(TriggerBox))
    {
        TriggerBox->SetGenerateOverlapEvents(true);
    }
}

void ACBossBattleStartTrigger::ResetTrigger()
{
    bHasTriggered = false;
    bIsEnabled = true;
    
    if (IsValid(TriggerBox))
    {
        TriggerBox->SetGenerateOverlapEvents(true);
        TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    }
    
    UE_LOG(LogTemp, Log, TEXT("[BossTrigger] 리스폰으로 트리거 초기화"));
}
