#include "CBossBattleStartTrigger.h"

#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "Components/BoxComponent.h"
#include "00_Character/01_Enemy/CEnemyBossCharacter.h"
#include "00_Character/00_Player/CPlayerCharacter.h"
#include "00_Stage/CStageManager.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
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

    if (IsValid(TargetBoss))
    {
        InitialBossName = TargetBoss->GetName();
    }
    
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
    GetWorld()->GetTimerManager().ClearTimer(WarningTimerHandle);
    GetWorld()->GetTimerManager().ClearTimer(SequenceTimerHandle);
    
    if (IsValid(TriggerBox))
    {
        TriggerBox->OnComponentBeginOverlap.RemoveDynamic(this, &ACBossBattleStartTrigger::OnTriggerBeginOverlap);
    }

    Super::EndPlay(EndPlayReason);
}

void ACBossBattleStartTrigger::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    ACPlayerCharacter* PlayerCharacter = Cast<ACPlayerCharacter>(OtherActor);
    if (!IsValid(PlayerCharacter))
    {
        return;
    }
    AttemptStartBossBattle(PlayerCharacter);
}

void ACBossBattleStartTrigger::AttemptStartBossBattle(ACPlayerCharacter* PlayerCharacter)
{
    if (!IsValid(PlayerCharacter))
    {
        return;
    }
    
    if (!bIsEnabled)
    {
        return;
    }

    if (bHasTriggered && bTriggerOnce)
    {
        return;
    }

    UE_LOG(LogTemp, Error, TEXT("[BossTrigger] 플레이어 감지 - 경고 UI 표시 시작"));

    if (!IsValid(TargetBoss))
    {
        UE_LOG(LogTemp, Warning, TEXT("[BossTrigger] TargetBoss가 유효하지 않음 - 재할당 시도"));
        
        TArray<AActor*> BossActors;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACEnemyBossCharacter::StaticClass(), BossActors);
        
        ACEnemyBossCharacter* ReacquiredBoss = nullptr;
        
        if (!InitialBossName.IsEmpty())
        {
            for (AActor* BossActor : BossActors)
            {
                if (BossActor && BossActor->GetName() == InitialBossName)
                {
                    ReacquiredBoss = Cast<ACEnemyBossCharacter>(BossActor);
                    break;
                }
            }
        }
        
        if (!ReacquiredBoss && BossActors.Num() == 1)
        {
            ReacquiredBoss = Cast<ACEnemyBossCharacter>(BossActors[0]);
        }
        
        if (ReacquiredBoss)
        {
            TargetBoss = ReacquiredBoss;
            UE_LOG(LogTemp, Warning, TEXT("[BossTrigger] TargetBoss 재설정: %s"), *GetNameSafe(TargetBoss));
        }
    }
    
    if (!IsValid(TargetBoss))
    {
        UE_LOG(LogTemp, Error, TEXT("[BossTrigger] TargetBoss 재할당 실패"));
        return;
    }

    CurrentPlayer = PlayerCharacter;
    ShowWarningUI();
    
    GetWorld()->GetTimerManager().SetTimer(
        SequenceTimerHandle,
        this,
        &ACBossBattleStartTrigger::PlayIntroSequence,
        WarningDisplayDuration + DelayBeforeSequence,
        false
    );
    
    bHasTriggered = true;
    
    if (bTriggerOnce)
    {
        bIsEnabled = false;
        if (IsValid(TriggerBox))
        {
            TriggerBox->SetGenerateOverlapEvents(false);
        }
    }
}

void ACBossBattleStartTrigger::ShowWarningUI()
{
    if (!WarningWidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[BossTrigger] WarningWidgetClass가 설정되지 않음"));
        return;
    }
    
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!IsValid(PC))
    {
        return;
    }
    
    UUserWidget* WarningWidget = CreateWidget<UUserWidget>(PC, WarningWidgetClass);
    if (WarningWidget)
    {
        WarningWidget->AddToViewport(100); 
        UE_LOG(LogTemp, Log, TEXT("[BossTrigger] 경고 UI 표시"));
        
        FTimerHandle RemoveWidgetTimer;
        GetWorld()->GetTimerManager().SetTimer(
            RemoveWidgetTimer,
            [WarningWidget]()
            {
                if (WarningWidget && WarningWidget->IsValidLowLevel())
                {
                    WarningWidget->RemoveFromParent();
                }
            },
            WarningDisplayDuration,
            false
        );
    }
}

void ACBossBattleStartTrigger::PlayIntroSequence()
{
    UE_LOG(LogTemp, Log, TEXT("[BossTrigger] 레벨 시퀀스 재생 시작"));
    
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (PC)
    {
        PlayerController = PC;
        PC->DisableInput(PC);
        
        PC->bShowMouseCursor = false;
        
        UE_LOG(LogTemp, Log, TEXT("[BossTrigger] 플레이어 입력 비활성화"));
    }
    
    if (CurrentPlayer.IsValid())
    {
        RepositionPlayer(CurrentPlayer.Get());
        
        if (UCharacterMovementComponent* MovementComp = CurrentPlayer->GetCharacterMovement())
        {
            MovementComp->StopMovementImmediately();
            MovementComp->DisableMovement();
        }
        
        if (USpringArmComponent* SpringArm = CurrentPlayer->GetCameraBoom())
        {
            bOriginalUsePawnControlRotation = SpringArm->bUsePawnControlRotation;
            SpringArm->bUsePawnControlRotation = false;  // 카메라 회전 고정
        }
        
        bOriginalUseControllerRotationYaw = CurrentPlayer->bUseControllerRotationYaw;
        CurrentPlayer->bUseControllerRotationYaw = false;
    }
    
    if (IntroSequence)
    {
        FMovieSceneSequencePlaybackSettings PlaybackSettings;
        PlaybackSettings.bAutoPlay = true;
        PlaybackSettings.bPauseAtEnd = false;
        
        ALevelSequenceActor* OutActor = nullptr;
        ULevelSequencePlayer* Player = ULevelSequencePlayer::CreateLevelSequencePlayer(
            GetWorld(),
            IntroSequence,
            PlaybackSettings,
            OutActor
        );
        
        if (Player && OutActor)
        {
            SequenceActor = OutActor;
            
            // 시퀀스 종료 시 콜백 바인딩
            Player->OnFinished.AddDynamic(this, &ACBossBattleStartTrigger::OnSequenceFinished);
            
            Player->Play();
            UE_LOG(LogTemp, Log, TEXT("[BossTrigger] 시퀀스 재생 중 - 카메라 회전 고정"));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[BossTrigger] 시퀀스 재생 실패 - 즉시 보스 전투 시작"));
            OnSequenceFinished();
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[BossTrigger] IntroSequence가 설정되지 않음 - 즉시 보스 전투 시작"));
        OnSequenceFinished();
    }
}

void ACBossBattleStartTrigger::RepositionPlayer(ACPlayerCharacter* PlayerCharacter)
{
    if (!IsValid(PlayerCharacter) || !IsValid(TargetBoss))
    {
        return;
    }
    
    FVector BossLocation = TargetBoss->GetActorLocation();
    FVector BossForward = TargetBoss->GetActorForwardVector();
    
    FVector TargetLocation = BossLocation + (BossForward * PlayerDistanceFromBoss);
    
    TargetLocation.Z = 82.0f;
    
    PlayerCharacter->SetActorLocation(TargetLocation);
    
    FRotator LookAtRotation = (BossLocation - TargetLocation).Rotation();
    FRotator PlayerRotation = FRotator(0.0f, LookAtRotation.Yaw, 0.0f);
    PlayerCharacter->SetActorRotation(PlayerRotation);
    
    if (APlayerController* PC = Cast<APlayerController>(PlayerCharacter->GetController()))
    {
        PC->SetControlRotation(PlayerRotation);
    }
    
    UE_LOG(LogTemp, Log, TEXT("[BossTrigger] 플레이어 재배치 완료 - 위치: %s, 회전: %s"), 
           *TargetLocation.ToString(), 
           *PlayerRotation.ToString());
}

void ACBossBattleStartTrigger::OnSequenceFinished()
{
    UE_LOG(LogTemp, Log, TEXT("[BossTrigger] 시퀀스 종료 - 보스 전투 시작"));
    
    if (PlayerController.IsValid())
    {
        PlayerController->EnableInput(PlayerController.Get());
        UE_LOG(LogTemp, Log, TEXT("[BossTrigger] 플레이어 입력 재활성화"));
    }
    
    if (CurrentPlayer.IsValid())
    {
        if (UCharacterMovementComponent* MovementComp = CurrentPlayer->GetCharacterMovement())
        {
            MovementComp->SetMovementMode(MOVE_Walking);
        }
        
        if (USpringArmComponent* SpringArm = CurrentPlayer->GetCameraBoom())
        {
            SpringArm->bUsePawnControlRotation = bOriginalUsePawnControlRotation;
        }
        
        CurrentPlayer->bUseControllerRotationYaw = bOriginalUseControllerRotationYaw;
        
        UE_LOG(LogTemp, Log, TEXT("[BossTrigger] 카메라 회전 입력 복원"));
    }
    
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACStageManager::StaticClass(), FoundActors);

    if (FoundActors.Num() > 0)
    {
        ACStageManager* StageManager = Cast<ACStageManager>(FoundActors[0]);
        if (IsValid(StageManager))
        {
            UE_LOG(LogTemp, Log, TEXT("[BossTrigger] StageManager에 보스 전투 시작 알림"));
            StageManager->OnBossBattleStartRequested();
        }
    }

    if (IsValid(TargetBoss))
    {
        TargetBoss->StartBossBattle(bSkipIntro);
        UE_LOG(LogTemp, Log, TEXT("[BossTrigger] 보스 전투 시작됨"));
    }
    
    if (SequenceActor)
    {
        SequenceActor->Destroy();
        SequenceActor = nullptr;
    }
    
    PlayerController.Reset();
    CurrentPlayer.Reset();
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
        SetTriggerEnabled(false);
    }
}

void ACBossBattleStartTrigger::SetTriggerEnabled(bool bEnabled)
{
    bIsEnabled = bEnabled;
    UE_LOG(LogTemp, Log, TEXT("[BossTrigger] 트리거 %s"), bEnabled ? TEXT("활성화") : TEXT("비활성화"));
        
    if (!IsValid(TriggerBox))
    {
        return;
    }
        
    TriggerBox->SetCollisionEnabled(bEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
    TriggerBox->SetGenerateOverlapEvents(bEnabled);
    
    if (bEnabled)
    {
        TriggerBox->UpdateOverlaps();
    }
}

/*void ACBossBattleStartTrigger::ResetTrigger()
{
    bHasTriggered = false;
    SetTriggerEnabled(true);
    
    if (IsValid(TriggerBox))
    {
        TriggerBox->UpdateOverlaps();
             
        TArray<AActor*> OverlappingActors;
        TriggerBox->GetOverlappingActors(OverlappingActors, ACPlayerCharacter::StaticClass());
             
        if (OverlappingActors.Num() > 0)
        {
            for (AActor* Actor : OverlappingActors)
            {
                if (ACPlayerCharacter* PlayerCharacter = Cast<ACPlayerCharacter>(Actor))
                {
                    UE_LOG(LogTemp, Log, TEXT("[BossTrigger] ResetTrigger - 플레이어가 이미 트리거 내부에 있음"));
                    AttemptStartBossBattle(PlayerCharacter);
                    break;
                }
            }
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("[BossTrigger] 리스폰으로 트리거 초기화"));
}*/

void ACBossBattleStartTrigger::ResetTrigger()
{
    // 상태 초기화
    bHasTriggered = false;
    bIsEnabled = true;
    
    if (IsValid(TriggerBox))
    {
        TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        TriggerBox->SetGenerateOverlapEvents(true);
        TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    }
    
    UE_LOG(LogTemp, Log, TEXT("[BossTrigger] 트리거 리셋 완료"));
}
