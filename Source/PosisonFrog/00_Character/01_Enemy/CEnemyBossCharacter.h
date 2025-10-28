#pragma once

#include "CoreMinimal.h"
#include "00_Character/CBaseCharacter.h"
#include "CEnemyBossCharacter.generated.h"

class UCEnemyBossPhaseComponent;
class UCEnemyHealthComponent;
class UCEnemyWeaponComponent;
struct FBossPhaseDefinition;
struct FBossPatternDefinition;

/** 보스 공통 캐릭터 베이스 */
UCLASS()
class POSISONFROG_API ACEnemyBossCharacter : public ACBaseCharacter
{
    GENERATED_BODY()

public:
    ACEnemyBossCharacter();

    virtual void BeginPlay() override;
    virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

    UFUNCTION(BlueprintCallable, Category="Boss")
    void StartBossBattle(bool bSkipIntro = false);

    UFUNCTION(BlueprintCallable, Category="Boss")
    void ForcePattern(FName PatternId);

    UFUNCTION(BlueprintPure, Category="Boss")
    UCEnemyBossPhaseComponent* GetBossPhaseComponent() const { return BossPhaseComponent; }

protected:
    void InitializeBossBindings();

    UFUNCTION()
    void HandlePhaseChanged(int32 PhaseIndex, const FBossPhaseDefinition& PhaseData);

    UFUNCTION()
    void HandlePatternStarted(int32 PhaseIndex, FName PatternId, const FBossPatternDefinition& PatternData, float RemainingPower);

    UFUNCTION()
    void HandlePatternFinished(int32 PhaseIndex, FName PatternId, const FBossPatternDefinition& PatternData, float RemainingPower);

    UFUNCTION()
    void HandleShoutStarted(int32 PhaseIndex, FName ShoutId, float Duration);

    UFUNCTION()
    void HandleShoutFinished(int32 PhaseIndex, FName ShoutId, float Duration);

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boss|Components")
    TObjectPtr<UCEnemyHealthComponent> HealthComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boss|Components")
    TObjectPtr<UCEnemyBossPhaseComponent> BossPhaseComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boss|Components")
    TObjectPtr<UCEnemyWeaponComponent> WeaponComponent;
};