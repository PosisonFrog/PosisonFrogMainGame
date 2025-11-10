// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "00_Character/02_Component/01_EnemyComponent/CBossPatternBase.h"
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

	virtual void ExecutePattern(int32 PhaseIndex) override;
	virtual void OnPatternEnd() override;
	virtual void BeginDestroy() override;  // 추가

protected:
	/** 공격 애니메이션 몽타주 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|BasicAttack")
	TObjectPtr<UAnimMontage> AttackMontage;

	/** 공격 인덱스 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern|BasicAttack")
	int32 AttackIndex;
};