#pragma once

#include "CoreMinimal.h"
#include "00_Character/CBaseCharacter.h"
#include "CEnemyBossCharacter.generated.h"

class UCEnemyBossPhaseComponent;
class UCEnemyHealthComponent;
class UCEnemyWeaponComponent;
struct FBossPhaseDefinition;
struct FBossPatternDefinition;
class UCBossPatternManager;
class UCBossPatternBase;

/** 보스 공통 캐릭터 베이스 */
UCLASS()
class POSISONFROG_API ACEnemyBossCharacter : public ACBaseCharacter
{
	GENERATED_BODY()

public:
	ACEnemyBossCharacter();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintCallable, Category="Boss")
	void StartBossBattle(bool bSkipIntro = false);

	UFUNCTION(BlueprintCallable, Category="Boss")
	void ForcePattern(FName PatternId);

	UFUNCTION(BlueprintCallable, Category="Boss")
	void ResetBossBattleState();
	
	UFUNCTION(BlueprintPure, Category="Boss")
	UCEnemyBossPhaseComponent* GetBossPhaseComponent() const { return BossPhaseComponent; }

	void SetIsBossRushing(bool bRushing) { bIsBossRushing = bRushing; }
	
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

	UFUNCTION()
	void HandleBossDeath(AActor* DeadActor);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss")
	bool bAutoStartBattle = false;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boss|Components")
	TObjectPtr<UCEnemyHealthComponent> HealthComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boss|Components")
	TObjectPtr<UCEnemyBossPhaseComponent> BossPhaseComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boss|Components")
	TObjectPtr<UCBossPatternManager> PatternManager;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boss|Components")
	TObjectPtr<UCEnemyWeaponComponent> WeaponComponent;
	
	// 패턴 컴포넌트들 (ActorComponent로 변경됨)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boss|Components|Patterns")
	TObjectPtr<UCBossPatternBase> BasicAttackPattern;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boss|Components|Patterns")
	TObjectPtr<UCBossPatternBase> BarragePattern;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boss|Components|Patterns")
	TObjectPtr<UCBossPatternBase> RushPattern;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boss|Components|Patterns")
	TObjectPtr<UCBossPatternBase> SlamPattern;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss|Pattern")
	TMap<FName, int32> PatternAttackIndexMap;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss|Pattern")
	int32 DefaultAttackIndex = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss|Rushing")
	bool bIsBossRushing = false;

	bool bIsBossDead = false;

	UPROPERTY(EditAnywhere, Category="Boss|Animation")
	TObjectPtr<UAnimMontage> DeathMontage;
};