// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "00_Character/02_Component/01_EnemyComponent/CBossPatternBase.h"
#include "03_Combat/Boss/BossPhaseDataAsset.h"
#include "CBossPattern_BasicAttack.generated.h"

/**
 * 기본 공격 패턴
 * 플레이어를 향해 근접 공격을 수행합니다.
 */
UCLASS(EditInlineNew, DefaultToInstanced)  // 수정: EditInlineNew, DefaultToInstanced 추가
class POSISONFROG_API UCBossPattern_BasicAttack : public UCBossPatternBase
{
	GENERATED_BODY()

public:
	UCBossPattern_BasicAttack();

	virtual void ExecutePattern(int32 PhaseIndex, const FBossPatternDefinition& PatternData) override;
	virtual void OnPatternEnd() override;
	virtual void BeginDestroy() override;
	
	/** 애님 노티파이: 공격 시작 (콜리전 활성화) */
	UFUNCTION(BlueprintCallable, Category = "Pattern|BasicAttack")
	void Anim_AttackStart();
	
	/** 애님 노티파이: 공격 종료 (콜리전 비활성화) */
	UFUNCTION(BlueprintCallable, Category = "Pattern|BasicAttack")
	void Anim_AttackEnd();

protected:
	/** 공격 애니메이션 몽타주 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|BasicAttack")
	TObjectPtr<UAnimMontage> AttackMontage;

	/** 공격 인덱스 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|BasicAttack")
	int32 AttackIndex;

	/** 오른손 소켓 이름 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|BasicAttack|Collision")
	FName RightHandSocketName = FName("hand_rSocket");
	
	/** 공격 판정 구체 반경 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|BasicAttack|Collision")
	float AttackSphereRadius = 80.0f;
	
	/** 기본공격 데미지 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|BasicAttack|Damage")
	float BasicAttackDamage = 30.0f;
	
	/** 넉백 강도 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|BasicAttack|Damage")
	float KnockbackPower = 800.0f;
	
	/** 넉백 상향 힘 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|BasicAttack|Damage")
	float KnockbackUpForce = 200.0f;
	
	/** 디버그 드로우 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|BasicAttack|Debug")
	bool bDrawDebug = false;

private:
	/** 현재 페이즈의 PatternData (ExecutionTime, RecoveryTime 등) */
	FBossPatternDefinition CurrentPatternData;
	
	/** 이미 데미지를 입힌 액터들 (중복 데미지 방지) */
	TSet<TWeakObjectPtr<AActor>> HitActors;
	
	/** 콜리전 활성화 여부 */
	bool bCollisionActive = false;
	
	/** 콜리전 체크 타이머 */
	FTimerHandle CollisionCheckTimer;
	
	/** 패턴 종료 타이머 */
	FTimerHandle FinishTimer;
	
	/** 콜리전 체크 함수 */
	void CheckCollision();
	
	/** 내부 패턴 종료 함수 (타이머에서 호출) */
	void FinishPatternInternal();
	
	/** 타이머 정리 */
	void ClearTimers();
};