#pragma once

#include "CoreMinimal.h"
#include "00_Character/02_Component/01_EnemyComponent/CBossPatternBase.h"
#include "03_Combat/Boss/BossPhaseDataAsset.h"
#include "CBossPattern_BasicAttack.generated.h"

/**
 * 기본 공격 패턴
 */
UCLASS(ClassGroup=(Boss), meta=(BlueprintSpawnableComponent))
class POSISONFROG_API UCBossPattern_BasicAttack : public UCBossPatternBase
{
	GENERATED_BODY()

public:
	UCBossPattern_BasicAttack();

	virtual bool ExecutePattern(int32 PhaseIndex, const FBossPatternDefinition& PatternData) override;
	virtual void OnPatternEnd() override;
	virtual void Cleanup() override;
	
	UFUNCTION(BlueprintCallable, Category = "Pattern|BasicAttack")
	void Anim_AttackStart();
	
	UFUNCTION(BlueprintCallable, Category = "Pattern|BasicAttack")
	void Anim_AttackEnd();

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|BasicAttack")
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Pattern|BasicAttack")
	int32 AttackIndex;

	UPROPERTY(EditDefaultsOnly, Category = "Pattern|BasicAttack|Collision")
	FName RightHandSocketName = FName("hand_rSocket");
	
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|BasicAttack|Collision")
	float AttackSphereRadius = 80.0f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|BasicAttack|Damage")
	float BasicAttackDamage = 30.0f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|BasicAttack|Damage")
	float KnockbackPower = 800.0f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|BasicAttack|Damage")
	float KnockbackUpForce = 200.0f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|BasicAttack|Debug")
	bool bDrawDebug = false;

private:
	FBossPatternDefinition CurrentPatternData;
	
	TSet<TWeakObjectPtr<AActor>> HitActors;
	
	bool bCollisionActive = false;
	
	FTimerHandle CollisionCheckTimer;
	FTimerHandle FinishTimer;
	
	void CheckCollision();
	void FinishPatternInternal();
	void ClearTimers();
};