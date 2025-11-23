#pragma once

#include "CoreMinimal.h"
#include "00_Character/02_Component/01_EnemyComponent/CBossPatternBase.h"
#include "03_Combat/Boss/BossPhaseDataAsset.h"
#include "CBossPattern_BasicAttack.generated.h"

/**
 * 기본 공격 패턴
 * AnimNotifyState_BossAttack을 통해 충돌 검사 구간을 제어합니다.
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
	
	// [노티파이 스테이트 연동 함수]
	void StartAttackCollision(); // 공격 판정 시작 (리스트 초기화)
	void CheckAttackCollision(); // 매 프레임 충돌 검사

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|BasicAttack")
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Pattern|BasicAttack")
	int32 AttackIndex;

	UPROPERTY(EditDefaultsOnly, Category = "Pattern|BasicAttack|Collision")
	FName RightHandSocketName = FName("hand_rSocket");
	
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|BasicAttack|Collision")
	float AttackSphereRadius = 300.0f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|BasicAttack|Damage")
	float BasicAttackDamage = 30.0f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|BasicAttack|Damage")
	float KnockbackPower = 2500.0f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|BasicAttack|Damage")
	float KnockbackUpForce = 300.0f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|BasicAttack|Debug")
	bool bDrawDebug = false;



private:

	FVector LockedAttackDirection = FVector::ForwardVector;
	FBossPatternDefinition CurrentPatternData;
	
	// 중복 피격 방지용
	TSet<TWeakObjectPtr<AActor>> HitActors;
	
	// 패턴 종료용 타이머
	FTimerHandle FinishTimer;
	
	void FinishPatternInternal();
	void ClearTimers();
};