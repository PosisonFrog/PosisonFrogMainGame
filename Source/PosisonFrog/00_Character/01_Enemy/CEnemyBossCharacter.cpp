#include "00_Character/01_Enemy/CEnemyBossCharacter.h"

#include "00_Character/02_Component/01_EnemyComponent/CEnemyBossPhaseComponent.h"
#include "00_Character/02_Component/01_EnemyComponent/CEnemyHealthComponent.h"
#include "00_Character/02_Component/01_EnemyComponent/CEnemyWeaponComponent.h"
#include "03_Combat/Boss/BossPhaseDataAsset.h"

ACEnemyBossCharacter::ACEnemyBossCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    HealthComponent = CreateDefaultSubobject<UCEnemyHealthComponent>(TEXT("HealthComponent"));
    BossPhaseComponent = CreateDefaultSubobject<UCEnemyBossPhaseComponent>(TEXT("BossPhaseComponent"));
    WeaponComponent = CreateDefaultSubobject<UCEnemyWeaponComponent>(TEXT("WeaponComponent"));
}

void ACEnemyBossCharacter::BeginPlay()
{
    Super::BeginPlay();

    InitializeBossBindings();
}

float ACEnemyBossCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    UE_LOG(LogTemp, Warning, TEXT("[%s] TakeDamage 호출됨! 데미지: %.1f, 공격자: %s"), 
           *GetName(), DamageAmount, *GetNameSafe(DamageCauser));
    
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

    if (BossPhaseComponent)
    {
        if (!BossPhaseComponent->IsBattleStarted())
        {
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
    if (BossPhaseComponent)
    {
        BossPhaseComponent->StartBattle(bSkipIntro);
    }
}

void ACEnemyBossCharacter::ForcePattern(FName PatternId)
{
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
