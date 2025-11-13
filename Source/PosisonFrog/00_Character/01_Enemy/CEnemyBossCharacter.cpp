#include "00_Character/01_Enemy/CEnemyBossCharacter.h"

#include "00_Character/02_Component/01_EnemyComponent/CBossPatternManager.h"
#include "00_Character/02_Component/01_EnemyComponent/CEnemyBossPhaseComponent.h"
#include "00_Character/02_Component/01_EnemyComponent/CEnemyHealthComponent.h"
#include "00_Character/02_Component/01_EnemyComponent/CEnemyWeaponComponent.h"
#include "00_Character/01_Enemy/01_AIController/BossAIController.h"
#include "03_Combat/Boss/BossPhaseDataAsset.h"
#include "AIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "05_System/01_Sound/CSoundManagerSubsystem.h"
#include "05_System/01_Sound//CSoundDataAsset.h"
#include "00_Character/CMainGameModeBase.h"

ACEnemyBossCharacter::ACEnemyBossCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
    
    HealthComponent = CreateDefaultSubobject<UCEnemyHealthComponent>(TEXT("HealthComponent"));
    BossPhaseComponent = CreateDefaultSubobject<UCEnemyBossPhaseComponent>(TEXT("BossPhaseComponent"));
    WeaponComponent = CreateDefaultSubobject<UCEnemyWeaponComponent>(TEXT("WeaponComponent"));
    PatternManager = CreateDefaultSubobject<UCBossPatternManager>(TEXT("PatternManager"));
}

void ACEnemyBossCharacter::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Error, TEXT("========================================"));
    UE_LOG(LogTemp, Error, TEXT("[Boss] BeginPlay - bAutoStartBattle = %s"), bAutoStartBattle ? TEXT("TRUE") : TEXT("FALSE"));
    UE_LOG(LogTemp, Error, TEXT("========================================"));

    if (bAutoStartBattle)
    {
        APawn* Player = GetWorld()->GetFirstPlayerController()
                      ? GetWorld()->GetFirstPlayerController()->GetPawn() 
                      : nullptr;
        if (Player)
        {
            UE_LOG(LogTemp, Error, TEXT("[Boss] AUTO START BATTLE TRIGGERED IN BEGINPLAY!"));
            StartBossBattle(false);
        }
    }
    
    bIsBossDead = false;

    if (IsValid(HealthComponent))
    {
        HealthComponent->OnDeath.AddDynamic(this, &ACEnemyBossCharacter::HandleBossDeath);
    }    
}

void ACEnemyBossCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    UE_LOG(LogTemp, Warning, TEXT("[Boss] EndPlay called"));
    
    // 패턴 매니저 클린업
    if (IsValid(PatternManager))
    {
        PatternManager->CleanupAllPatterns();
    }
    
    // 모든 타이머 정리
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearAllTimersForObject(this);
    }
    
    if (IsValid(HealthComponent))
    {
        HealthComponent->OnDeath.RemoveDynamic(this, &ACEnemyBossCharacter::HandleBossDeath);
    }
    
    Super::EndPlay(EndPlayReason);
}

float ACEnemyBossCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    UE_LOG(LogTemp, Warning, TEXT("[%s] TakeDamage 호출됨! 데미지: %.1f, 공격자: %s"), 
           *GetName(), DamageAmount, *GetNameSafe(DamageCauser));
    
    // 이미 죽었으면 데미지 무시
    if (bIsBossDead)
    {
        UE_LOG(LogTemp, Log, TEXT("[Boss] Already dead, ignoring damage"));
        return 0.0f;
    }
    
    const float AppliedDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    if (AppliedDamage <= 0.f)
    {
        return AppliedDamage;
    }

    if (HealthComponent)
    {
        float OldHealth = HealthComponent->GetHealth();
        HealthComponent->Damage(AppliedDamage);
        float NewHealth = HealthComponent->GetHealth();
        
        UE_LOG(LogTemp, Warning, TEXT("[%s] 체력 변화: %.1f -> %.1f"), 
               *GetName(), OldHealth, NewHealth);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[%s] HealthComponent를 찾을 수 없음!"), *GetName());
    }
    
    const bool bIsDead = IsValid(HealthComponent) && HealthComponent->IsDead();
    if (bIsDead)
    {
        bIsBossDead = true;
        // HandleBossDeath가 델리게이트로 호출됨
        return AppliedDamage;
    }
    
    if (BossPhaseComponent)
    {
        if (BossPhaseComponent->GetCurrentState() == EBossBattleState::Dead)
        {
            return AppliedDamage;
        }

        if (!BossPhaseComponent->IsBattleStarted())
        {
            UE_LOG(LogTemp, Error, TEXT("[Boss] AUTO START BATTLE TRIGGERED BY TAKEDAMAGE!"));
            BossPhaseComponent->StartBattle();
        }

        if (const UBossPhaseDataAsset* PhaseData = BossPhaseComponent->PhaseData)
        {
            BossPhaseComponent->AddPower(PhaseData->PowerGainOnHit);
        }
    }

    return AppliedDamage;
}

void ACEnemyBossCharacter::StartBossBattle(bool bSkipIntro)
{
    UE_LOG(LogTemp, Error, TEXT("========================================"));
    UE_LOG(LogTemp, Error, TEXT("[Boss] StartBossBattle CALLED! SkipIntro=%s"), bSkipIntro ? TEXT("true") : TEXT("false"));
    UE_LOG(LogTemp, Error, TEXT("[Boss] Call Location: %s"), ANSI_TO_TCHAR(__FUNCTION__));
    UE_LOG(LogTemp, Error, TEXT("========================================"));
    
    if (bIsBossDead)
    {
        UE_LOG(LogTemp, Verbose, TEXT("[Boss] StartBossBattle ignored because boss is dead"));
        return;
    }
    
    if (BossPhaseComponent)
    {
        BossPhaseComponent->StartBattle(bSkipIntro);
    }

    if (ACMainGameModeBase* GM = Cast<ACMainGameModeBase>(GetWorld()->GetAuthGameMode()))
    {
        GM->PlayBossBGM();
    }
}

void ACEnemyBossCharacter::ForcePattern(FName PatternId)
{
    if (bIsBossDead)
    {
        UE_LOG(LogTemp, Verbose, TEXT("[Boss] ForcePattern(%s) ignored because boss is dead"), *PatternId.ToString());
        return;
    }
    if (BossPhaseComponent)
    {
        BossPhaseComponent->ForceNextPattern(PatternId);
    }
}

void ACEnemyBossCharacter::InitializeBossBindings()
{
    if (!BossPhaseComponent)
    {
        return;
    }

    BossPhaseComponent->OnPhaseChanged.AddDynamic(this, &ACEnemyBossCharacter::HandlePhaseChanged);
    BossPhaseComponent->OnPatternStarted.AddDynamic(this, &ACEnemyBossCharacter::HandlePatternStarted);
    BossPhaseComponent->OnPatternFinished.AddDynamic(this, &ACEnemyBossCharacter::HandlePatternFinished);
    BossPhaseComponent->OnShoutStarted.AddDynamic(this, &ACEnemyBossCharacter::HandleShoutStarted);
    BossPhaseComponent->OnShoutFinished.AddDynamic(this, &ACEnemyBossCharacter::HandleShoutFinished);
}

void ACEnemyBossCharacter::HandlePhaseChanged(int32 PhaseIndex, const FBossPhaseDefinition& PhaseData)
{
    UE_LOG(LogTemp, Log, TEXT("[Boss] Phase changed to %s (Index %d)"), *PhaseData.PhaseName.ToString(), PhaseIndex);
}

void ACEnemyBossCharacter::HandlePatternStarted(int32 PhaseIndex, FName PatternId, const FBossPatternDefinition& PatternData, float RemainingPower)
{
    UE_LOG(LogTemp, Log, TEXT("[Boss] Pattern %s started (Phase %d, Power %.1f)"), *PatternId.ToString(), PhaseIndex, RemainingPower);

    if (bIsBossDead || (IsValid(BossPhaseComponent) && BossPhaseComponent->GetCurrentState() == EBossBattleState::Dead))
    {
        UE_LOG(LogTemp, Verbose, TEXT("[Boss] Pattern %s ignored because boss is dead"), *PatternId.ToString());
        return;
    }
    
    if (IsValid(HealthComponent) && HealthComponent->IsDead())
    {
        bIsBossDead = true;
        UE_LOG(LogTemp, Verbose, TEXT("[Boss] Pattern %s ignored due to dead health state"), *PatternId.ToString());
        return;
    }
}

void ACEnemyBossCharacter::HandlePatternFinished(int32 PhaseIndex, FName PatternId, const FBossPatternDefinition& PatternData, float RemainingPower)
{
    UE_LOG(LogTemp, Log, TEXT("[Boss] Pattern %s finished (Phase %d, Power %.1f)"), *PatternId.ToString(), PhaseIndex, RemainingPower);
}

void ACEnemyBossCharacter::HandleShoutStarted(int32 PhaseIndex, FName ShoutId, float Duration)
{
    UE_LOG(LogTemp, Warning, TEXT("[Boss] Shout %s started (Phase %d, Duration %.2fs)"), *ShoutId.ToString(), PhaseIndex, Duration);
}

void ACEnemyBossCharacter::HandleShoutFinished(int32 PhaseIndex, FName ShoutId, float Duration)
{
    UE_LOG(LogTemp, Warning, TEXT("[Boss] Shout %s finished (Phase %d)"), *ShoutId.ToString(), PhaseIndex);
}

void ACEnemyBossCharacter::HandleBossDeath(AActor* DeadActor)
{
    if (DeadActor != this)
    {
        return;
    }
  
    UE_LOG(LogTemp, Error, TEXT("[Boss] ========== BOSS DEATH =========="));
    
    bIsBossDead = true;

    // 패턴 매니저 클린업
    if (IsValid(PatternManager))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Boss] Cleaning up pattern manager"));
        PatternManager->CleanupAllPatterns();
    }

    // AI 정지
    if (AAIController* AIController = Cast<AAIController>(GetController()))
    {
        AIController->StopMovement();
        
        if (ABossAIController* BossAI = Cast<ABossAIController>(AIController))
        {
            BossAI->SetChaseEnabled(false);
            BossAI->SetTargetPlayer(nullptr);
        }
    }
    
    // 이동 정지
    if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
    {
        MovementComponent->StopMovementImmediately();
    }
    
    // Tick 비활성화
    SetActorTickEnabled(false);

    // 애니메이션 처리
    if (USkeletalMeshComponent* mesh = GetMesh())
    {
        if (UAnimInstance* AnimInst = mesh->GetAnimInstance())
        {
            AnimInst->StopAllMontages(0.f); 
            
            if (DeathMontage)
            {
                AnimInst->Montage_Play(DeathMontage, 1.0f);
                UE_LOG(LogTemp, Warning, TEXT("[Boss] Playing death montage: %s"), *DeathMontage->GetName());
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("[Boss] No death montage assigned!"));
            }
        }
    }
    
    // 모든 타이머 정리
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearAllTimersForObject(this);
    }
    
    UE_LOG(LogTemp, Error, TEXT("[Boss] ========== BOSS DEATH COMPLETE =========="));
}