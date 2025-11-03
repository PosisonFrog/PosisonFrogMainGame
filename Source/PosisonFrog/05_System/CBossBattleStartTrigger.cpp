#include "CBossBattleStartTrigger.h"
#include "Components/BoxComponent.h"
#include "00_Character/01_Enemy/CEnemyBossCharacter.h"
#include "00_Character/00_Player/CPlayerCharacter.h"

ACBossBattleStartTrigger::ACBossBattleStartTrigger()
{
    PrimaryActorTick.bCanEverTick = false;

    // 루트 컴포넌트로 박스 컴포넌트 생성
    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    RootComponent = TriggerBox;

    // 박스 기본 설정
    TriggerBox->SetBoxExtent(FVector(200.0f, 200.0f, 200.0f));
    TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    TriggerBox->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
    TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    TriggerBox->SetGenerateOverlapEvents(true);
    
    UE_LOG(LogTemp, Warning, TEXT("[BossTrigger] Constructor - TriggerBox created"));

#if WITH_EDITORONLY_DATA
    // 에디터에서 보이도록 설정
    TriggerBox->bHiddenInGame = false;
    TriggerBox->SetVisibility(true);
#endif
}

void ACBossBattleStartTrigger::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Error, TEXT("[BossTrigger] ========================================"));
    UE_LOG(LogTemp, Error, TEXT("[BossTrigger] %s initialized"), *GetName());
    UE_LOG(LogTemp, Error, TEXT("[BossTrigger] bIsEnabled: %s"), bIsEnabled ? TEXT("TRUE") : TEXT("FALSE"));
    UE_LOG(LogTemp, Error, TEXT("[BossTrigger] bTriggerOnce: %s"), bTriggerOnce ? TEXT("TRUE") : TEXT("FALSE"));
    UE_LOG(LogTemp, Error, TEXT("[BossTrigger] bSkipIntro: %s"), bSkipIntro ? TEXT("TRUE") : TEXT("FALSE"));
    UE_LOG(LogTemp, Error, TEXT("[BossTrigger] TargetBoss: %s"), *GetNameSafe(TargetBoss));
    UE_LOG(LogTemp, Error, TEXT("[BossTrigger] ========================================"));

    if (IsValid(TriggerBox))
    {
        TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ACBossBattleStartTrigger::OnTriggerBeginOverlap);
    }

    // 보스 레퍼런스 유효성 검사
    if (!IsValid(TargetBoss))
    {
        UE_LOG(LogTemp, Warning, TEXT("[BossTrigger] %s: TargetBoss가 설정되지 않았습니다!"), *GetName());
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
    UE_LOG(LogTemp, Warning, TEXT("[BossTrigger] OnTriggerBeginOverlap - Actor: %s"), *GetNameSafe(OtherActor));
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
        UE_LOG(LogTemp, Verbose, TEXT("[BossTrigger] Not a player character, ignoring"));
        return;
    }
    
    UE_LOG(LogTemp, Error, TEXT("[BossTrigger] ✅ PLAYER DETECTED! Starting boss battle..."));

    // 보스 레퍼런스 유효성 확인
    if (!IsValid(TargetBoss))
    {
        UE_LOG(LogTemp, Error, TEXT("[BossTrigger] %s: TargetBoss가 유효하지 않습니다!"), *GetName());
        return;
    }

    // 보스 전투 시작
    UE_LOG(LogTemp, Error, TEXT("[BossTrigger] ========================================"));
    UE_LOG(LogTemp, Error, TEXT("[BossTrigger] 🎬 BATTLE START TRIGGERED!"));
    UE_LOG(LogTemp, Error, TEXT("[BossTrigger] Player: %s"), *GetNameSafe(PlayerCharacter));
    UE_LOG(LogTemp, Error, TEXT("[BossTrigger] Boss: %s"), *GetNameSafe(TargetBoss));
    UE_LOG(LogTemp, Error, TEXT("[BossTrigger] SkipIntro: %s"), bSkipIntro ? TEXT("true") : TEXT("false"));
    UE_LOG(LogTemp, Error, TEXT("[BossTrigger] ========================================"));

    TargetBoss->StartBossBattle(bSkipIntro);
    bHasTriggered = true;

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
    UE_LOG(LogTemp, Warning, TEXT("[BossTrigger] ManualTrigger called"));
    
    if (!IsValid(TargetBoss))
    {
        UE_LOG(LogTemp, Error, TEXT("[BossTrigger] ManualTrigger failed - TargetBoss is null"));
        return;
    }
    
    if (bHasTriggered && bTriggerOnce)
    {
        UE_LOG(LogTemp, Warning, TEXT("[BossTrigger] ManualTrigger ignored - Already triggered"));
        return;
    }
    
    UE_LOG(LogTemp, Error, TEXT("[BossTrigger] 🎬 MANUAL TRIGGER - Starting boss battle"));
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
    UE_LOG(LogTemp, Log, TEXT("[BossTrigger] Trigger %s"), bEnabled ? TEXT("ENABLED") : TEXT("DISABLED"));
    
    if (!bEnabled && IsValid(TriggerBox))
    {
        TriggerBox->SetGenerateOverlapEvents(false);
    }
    else if (bEnabled && IsValid(TriggerBox))
    {
        TriggerBox->SetGenerateOverlapEvents(true);
    }
}